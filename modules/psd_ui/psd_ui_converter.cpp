/**************************************************************************/
/*  psd_ui_converter.cpp                                                  */
/**************************************************************************/

#include "psd_ui_converter.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "core/io/resource_saver.h"
#include "core/math/math_funcs.h"
#include "core/os/memory.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/packed_scene.h"

#include <string>

// psd_sdk headers
#include "Psd.h"
#include "PsdMallocAllocator.h"
#include "PsdNativeFile.h"
#include "PsdDocument.h"
#include "PsdColorMode.h"
#include "PsdLayer.h"
#include "PsdChannel.h"
#include "PsdChannelType.h"
#include "PsdLayerMaskSection.h"
#include "PsdLayerType.h"
#include "PsdParseDocument.h"
#include "PsdParseLayerMaskSection.h"
#include "PsdLayerCanvasCopy.h"
#include "PsdInterleave.h"

using namespace psd;

static const unsigned int CHANNEL_NOT_FOUND = UINT_MAX;

static unsigned int FindChannel(Layer *layer, int16_t channelType) {
	for (unsigned int i = 0; i < layer->channelCount; ++i) {
		Channel *channel = &layer->channels[i];
		if (channel->data && channel->type == channelType)
			return i;
	}
	return CHANNEL_NOT_FOUND;
}

Error PsdUiConverter::parse(const String &p_psd_path, const Options &p_options, Vector<PsdLayerInfo> &r_layers, Size2i &r_doc_size) {
	r_layers.clear();
	r_doc_size = Size2i();

	// Convert Godot virtual path to absolute filesystem path for psd_sdk.
	String abs_path = ProjectSettings::get_singleton()->globalize_path(p_psd_path);
	Char16String utf16_path = abs_path.utf16();
	std::wstring wpath(reinterpret_cast<const wchar_t *>(utf16_path.get_data()));

	MallocAllocator allocator;
	NativeFile file(&allocator);

	if (!file.OpenRead(wpath.c_str())) {
		ERR_PRINT("Cannot open PSD file: " + p_psd_path);
		return ERR_CANT_OPEN;
	}

	Document *document = CreateDocument(&file, &allocator);
	if (!document) {
		ERR_PRINT("Cannot parse PSD document: " + p_psd_path);
		file.Close();
		return ERR_PARSE_ERROR;
	}

	if (document->colorMode != colorMode::RGB) {
		ERR_PRINT("PSD document is not in RGB color mode: " + p_psd_path);
		DestroyDocument(document, &allocator);
		file.Close();
		return ERR_INVALID_DATA;
	}

	if (document->bitsPerChannel != 8) {
		ERR_PRINT("PSD document must be 8 bits per channel: " + p_psd_path);
		DestroyDocument(document, &allocator);
		file.Close();
		return ERR_INVALID_DATA;
	}

	LayerMaskSection *layerMaskSection = ParseLayerMaskSection(document, &file, &allocator);
	if (!layerMaskSection) {
		ERR_PRINT("Cannot parse PSD layer mask section: " + p_psd_path);
		DestroyDocument(document, &allocator);
		file.Close();
		return ERR_PARSE_ERROR;
	}

	r_doc_size = Size2i(static_cast<int>(document->width), static_cast<int>(document->height));

	HashMap<Layer *, int> layer_to_index;

	// First pass: collect layer info, extract images and text.
	for (unsigned int i = 0; i < layerMaskSection->layerCount; ++i) {
		Layer *layer = &layerMaskSection->layers[i];
		ExtractLayer(document, &file, &allocator, layer);

		// Skip section dividers (they're just structural markers).
		if (layer->type == layerType::SECTION_DIVIDER) {
			continue;
		}

		PsdLayerInfo info;
		info.left = layer->left;
		info.top = layer->top;
		info.right = layer->right;
		info.bottom = layer->bottom;
		info.is_visible = layer->isVisible;
		info.is_group = (layer->type == layerType::OPEN_FOLDER || layer->type == layerType::CLOSED_FOLDER);

		// Layer name.
		if (layer->utf16Name) {
			info.name = String::utf16(reinterpret_cast<const char16_t *>(layer->utf16Name));
		} else {
			info.name = String(layer->name.c_str());
		}
		if (info.name.is_empty()) {
			info.name = "Layer_" + itos(i);
		}

		// Skip hidden layers if requested.
		if (p_options.skip_hidden && !info.is_visible) {
			continue;
		}

		// For non-group layers, extract pixel data (text layers carry a rasterized bitmap too).
		if (!info.is_group) {
			Ref<Image> img = _extract_layer_image(document, layer);
			if (img.is_valid() && img->get_width() > 0 && img->get_height() > 0) {
				info.image = img;
			}
		}

		// Text layer content and styling (parsed from the 'TySh' block by psd_sdk).
		if (layer->isText && layer->textContent) {
			info.is_text = true;
			info.text = String::utf16(reinterpret_cast<const char16_t *>(layer->textContent));
			info.font_size = layer->fontSize;
			info.font_color = Color(layer->fontColor[0], layer->fontColor[1], layer->fontColor[2], layer->fontColor[3]);
			if (layer->fontName) {
				info.font_name = String::utf16(reinterpret_cast<const char16_t *>(layer->fontName));
			}

			// Fallback when EngineData styling could not be parsed: estimate size from the bounding box height
			// and sample the font color from the rasterized text bitmap.
			if (info.font_size <= 0.0) {
				const float h = static_cast<float>(info.bottom - info.top);
				info.font_size = MAX(8.0f, h * 0.8f);
				if (info.image.is_valid()) {
					info.font_color = _sample_dominant_color(info.image);
				}
			}
		}

		const int idx = r_layers.size();
		layer_to_index[layer] = idx;
		r_layers.push_back(info);
	}

	// Second pass: establish parent relationships.
	for (unsigned int i = 0; i < layerMaskSection->layerCount; ++i) {
		Layer *layer = &layerMaskSection->layers[i];
		if (layer->type == layerType::SECTION_DIVIDER) {
			continue;
		}

		HashMap<Layer *, int>::Iterator E = layer_to_index.find(layer);
		if (!E) {
			continue;
		}

		const int child_idx = E->value;
		if (layer->parent) {
			HashMap<Layer *, int>::Iterator PE = layer_to_index.find(layer->parent);
			if (PE) {
				r_layers.write[child_idx].parent_index = PE->value;
			}
		}
	}

	DestroyLayerMaskSection(layerMaskSection, &allocator);
	DestroyDocument(document, &allocator);
	file.Close();

	return OK;
}

Control *PsdUiConverter::build_scene(const Vector<PsdLayerInfo> &p_layers, const Size2i &p_doc_size, const Options &p_options, TextureMode p_mode, const HashMap<int, Ref<Texture2D>> *p_external_textures, const HashMap<int, Ref<PackedScene>> *p_external_scenes) {
	Control *root = memnew(Control);
	root->set_name("PsdRoot");
	if (p_options.root_size) {
		root->set_custom_minimum_size(Size2(static_cast<float>(p_doc_size.x), static_cast<float>(p_doc_size.y)));
		root->set_size(Size2(static_cast<float>(p_doc_size.x), static_cast<float>(p_doc_size.y)));
	}

	const int count = p_layers.size();

	// Phase A: create node objects (no attachment yet) so parent/child order in the PSD does not matter.
	HashMap<int, Node *> node_map;
	for (int i = 0; i < count; ++i) {
		const PsdLayerInfo &info = p_layers[i];

		// Position relative to the parent group (PSD coordinates are absolute / document-space).
		Vector2 pos(static_cast<float>(info.left), static_cast<float>(info.top));
		if (info.parent_index >= 0 && info.parent_index < count) {
			const PsdLayerInfo &parent = p_layers[info.parent_index];
			pos = Vector2(static_cast<float>(info.left - parent.left), static_cast<float>(info.top - parent.top));
		}
		const Vector2 size(static_cast<float>(info.right - info.left), static_cast<float>(info.bottom - info.top));

		Node *node = nullptr;

		// Public tscn substitution: replace this layer with an instance of a common scene. Its PSD child layers
		// are preserved and get parented under the instance in Phase B.
		if (p_external_scenes) {
			HashMap<int, Ref<PackedScene>>::ConstIterator sit = p_external_scenes->find(i);
			if (sit && sit->value.is_valid()) {
				Node *inst = sit->value->instantiate();
				if (inst) {
					inst->set_name(sanitize_name(info.name));
					Control *inst_ctrl = Object::cast_to<Control>(inst);
					if (inst_ctrl) {
						inst_ctrl->set_position(pos);
					}
					node = inst;
				}
			}
		}

		if (node != nullptr) {
			// Already created as a common-scene instance above.
		} else if (info.is_group) {
			Control *ctrl = memnew(Control);
			ctrl->set_name(sanitize_name(info.name));
			ctrl->set_position(pos);
			ctrl->set_size(size);
			node = ctrl;
		} else if (info.is_text && p_options.text_as_label) {
			Label *label = memnew(Label);
			label->set_name(sanitize_name(info.name));
			label->set_position(pos);
			label->set_size(size);
			label->set_text(info.text);
			if (info.font_size > 0.0) {
				label->add_theme_font_size_override("font_size", static_cast<int>(Math::round(info.font_size)));
			}
			label->add_theme_color_override("font_color", info.font_color);
			node = label;
		} else {
			// Image layer (or a text layer rendered as an image when text_as_label is disabled).
			Ref<Texture2D> tex;
			if (p_mode == TEXTURE_EXTERNAL && p_external_textures) {
				HashMap<int, Ref<Texture2D>>::ConstIterator it = p_external_textures->find(i);
				if (it) {
					tex = it->value;
				}
			}
			if (tex.is_null() && info.image.is_valid()) {
				tex = ImageTexture::create_from_image(info.image);
			}

			if (tex.is_valid()) {
				TextureRect *tr = memnew(TextureRect);
				tr->set_name(sanitize_name(info.name));
				tr->set_position(pos);
				tr->set_size(size);
				tr->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
				tr->set_texture(tex);
				node = tr;
			} else {
				Control *ctrl = memnew(Control);
				ctrl->set_name(sanitize_name(info.name));
				ctrl->set_position(pos);
				ctrl->set_size(size);
				node = ctrl;
			}
		}

		node_map[i] = node;
	}

	// Phase B: attach nodes to their parent in dependency order (a node is attached only after its parent is),
	// so that set_owner(root) always sees root as an ancestor regardless of PSD layer ordering.
	Vector<bool> attached;
	attached.resize(count);
	for (int i = 0; i < count; ++i) {
		attached.write[i] = false;
	}

	bool progress = true;
	int remaining = count;
	while (progress && remaining > 0) {
		progress = false;
		for (int i = 0; i < count; ++i) {
			if (attached[i]) {
				continue;
			}
			const PsdLayerInfo &info = p_layers[i];
			const bool parent_ready = (info.parent_index < 0) || (info.parent_index < count && attached[info.parent_index]);
			if (!parent_ready) {
				continue;
			}

			Node *parent_node = root;
			if (info.parent_index >= 0) {
				HashMap<int, Node *>::Iterator pit = node_map.find(info.parent_index);
				if (pit) {
					parent_node = pit->value;
				}
			}
			parent_node->add_child(node_map[i], false, Node::INTERNAL_MODE_DISABLED);
			node_map[i]->set_owner(root);
			attached.write[i] = true;
			--remaining;
			progress = true;
		}
	}

	// Any leftover nodes (e.g. dangling parent references) are attached directly to the root.
	for (int i = 0; i < count; ++i) {
		if (!attached[i]) {
			root->add_child(node_map[i], false, Node::INTERNAL_MODE_DISABLED);
			node_map[i]->set_owner(root);
		}
	}

	return root;
}

Error PsdUiConverter::convert(const String &p_psd_path, const String &p_output_scene_path, const Options &p_options) {
	Vector<PsdLayerInfo> layers;
	Size2i doc_size;
	Error err = parse(p_psd_path, p_options, layers, doc_size);
	if (err != OK) {
		return err;
	}

	// ResourceImporter path: textures are embedded so the imported scene is self-contained.
	Control *root = build_scene(layers, doc_size, p_options, TEXTURE_EMBED, nullptr);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	err = packed_scene->pack(root);
	if (err != OK) {
		ERR_PRINT("Failed to pack PSD scene.");
		memdelete(root);
		return err;
	}

	err = ResourceSaver::save(packed_scene, p_output_scene_path);
	if (err != OK) {
		ERR_PRINT("Failed to save PSD scene: " + p_output_scene_path);
	}

	memdelete(root);
	return err;
}

Ref<Image> PsdUiConverter::_extract_layer_image(void *p_document, void *p_layer) {
	Document *document = static_cast<Document *>(p_document);
	Layer *layer = static_cast<Layer *>(p_layer);

	if (document->bitsPerChannel != 8) {
		return Ref<Image>();
	}

	const unsigned int indexR = FindChannel(layer, channelType::R);
	const unsigned int indexG = FindChannel(layer, channelType::G);
	const unsigned int indexB = FindChannel(layer, channelType::B);
	const unsigned int indexA = FindChannel(layer, channelType::TRANSPARENCY_MASK);

	// Need at least RGB channels.
	if (indexR == CHANNEL_NOT_FOUND || indexG == CHANNEL_NOT_FOUND || indexB == CHANNEL_NOT_FOUND) {
		return Ref<Image>();
	}

	// Use layer's actual pixel dimensions for efficiency.
	unsigned int layerWidth = static_cast<unsigned int>(layer->right - layer->left);
	unsigned int layerHeight = static_cast<unsigned int>(layer->bottom - layer->top);
	if (layerWidth == 0 || layerHeight == 0) {
		return Ref<Image>();
	}

	const uint8_t *srcR = static_cast<const uint8_t *>(layer->channels[indexR].data);
	const uint8_t *srcG = static_cast<const uint8_t *>(layer->channels[indexG].data);
	const uint8_t *srcB = static_cast<const uint8_t *>(layer->channels[indexB].data);
	const uint8_t *srcA = nullptr;
	if (indexA != CHANNEL_NOT_FOUND) {
		srcA = static_cast<const uint8_t *>(layer->channels[indexA].data);
	}

	unsigned int pixelCount = layerWidth * layerHeight;
	unsigned int imageSize = pixelCount * 4;

	uint8_t *alignedR = static_cast<uint8_t *>(Memory::alloc_aligned_static(pixelCount, 16));
	uint8_t *alignedG = static_cast<uint8_t *>(Memory::alloc_aligned_static(pixelCount, 16));
	uint8_t *alignedB = static_cast<uint8_t *>(Memory::alloc_aligned_static(pixelCount, 16));
	uint8_t *alignedA = nullptr;
	uint8_t *interleaved = static_cast<uint8_t *>(Memory::alloc_aligned_static(imageSize, 16));

	if (!alignedR || !alignedG || !alignedB || !interleaved) {
		if (alignedR) Memory::free_aligned_static(alignedR);
		if (alignedG) Memory::free_aligned_static(alignedG);
		if (alignedB) Memory::free_aligned_static(alignedB);
		if (interleaved) Memory::free_aligned_static(interleaved);
		return Ref<Image>();
	}

	memcpy(alignedR, srcR, pixelCount);
	memcpy(alignedG, srcG, pixelCount);
	memcpy(alignedB, srcB, pixelCount);

	if (srcA) {
		alignedA = static_cast<uint8_t *>(Memory::alloc_aligned_static(pixelCount, 16));
		memcpy(alignedA, srcA, pixelCount);
		imageUtil::InterleaveRGBA(alignedR, alignedG, alignedB, alignedA, interleaved, layerWidth, layerHeight);
	} else {
		imageUtil::InterleaveRGB(alignedR, alignedG, alignedB, 255, interleaved, layerWidth, layerHeight);
	}

	Memory::free_aligned_static(alignedR);
	Memory::free_aligned_static(alignedG);
	Memory::free_aligned_static(alignedB);
	if (alignedA) {
		Memory::free_aligned_static(alignedA);
	}

	Vector<uint8_t> image_data;
	image_data.resize(imageSize);
	memcpy(image_data.ptrw(), interleaved, imageSize);
	Ref<Image> img = Image::create_from_data(layerWidth, layerHeight, false, Image::FORMAT_RGBA8, image_data);

	Memory::free_aligned_static(interleaved);
	return img;
}

Color PsdUiConverter::_sample_dominant_color(const Ref<Image> &p_image) {
	if (p_image.is_null()) {
		return Color(0, 0, 0, 1);
	}
	const int w = p_image->get_width();
	const int h = p_image->get_height();
	if (w <= 0 || h <= 0) {
		return Color(0, 0, 0, 1);
	}

	// Average the opaque pixels over a bounded grid to find the (text) color.
	const int step_x = MAX(1, w / 64);
	const int step_y = MAX(1, h / 64);
	double r = 0.0, g = 0.0, b = 0.0;
	int sample_count = 0;
	for (int y = 0; y < h; y += step_y) {
		for (int x = 0; x < w; x += step_x) {
			const Color c = p_image->get_pixel(x, y);
			if (c.a > 0.5) {
				r += c.r;
				g += c.g;
				b += c.b;
				++sample_count;
			}
		}
	}
	if (sample_count == 0) {
		return Color(0, 0, 0, 1);
	}
	return Color(r / sample_count, g / sample_count, b / sample_count, 1.0);
}

String PsdUiConverter::sanitize_name(const String &p_name) {
	String result = p_name;
	// Replace characters that are invalid for Godot node names.
	result = result.replace("/", "_");
	result = result.replace("\\", "_");
	result = result.replace(":", "_");
	result = result.replace("@", "_");
	result = result.replace(".", "_");
	result = result.replace(" ", "_");
	if (result.is_empty()) {
		result = "Layer";
	}
	return result;
}
