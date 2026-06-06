/**************************************************************************/
/*  psd_ui_converter.h                                                    */
/**************************************************************************/
#pragma once

#include "core/io/image.h"
#include "core/math/color.h"
#include "core/math/vector2i.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "scene/gui/control.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

struct PsdLayerInfo {
	String name;
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	bool is_visible = true;
	bool is_group = false;
	int parent_index = -1;
	Ref<Image> image;

	// Text layer info (valid when is_text is true).
	bool is_text = false;
	String text;
	double font_size = 0.0;
	Color font_color = Color(0, 0, 0, 1);
	String font_name;
};

class PsdUiConverter {
public:
	struct Options {
		bool skip_hidden = true;
		bool root_size = true;
		bool text_as_label = true;
		String output_folder;
	};

	enum TextureMode {
		TEXTURE_EMBED, // embed an ImageTexture built from each layer image (self-contained scene)
		TEXTURE_EXTERNAL, // reference pre-loaded external textures via the provided map
	};

	// Pure parse step: open and parse the PSD file, filling the layer list and document size.
	// Does not touch nodes, the filesystem, or the editor.
	static Error parse(const String &p_psd_path, const Options &p_options, Vector<PsdLayerInfo> &r_layers, Size2i &r_doc_size);

	// Builds a Control node tree from parsed layers. The caller owns the returned root and is responsible for
	// packing/saving and freeing it. In TEXTURE_EXTERNAL mode, p_external_textures maps a layer index to a
	// pre-loaded Texture2D (typically an imported res:// PNG); indices without an entry fall back to embedding.
	// p_external_scenes maps a layer index to a PackedScene: that layer becomes an instance of the scene, while
	// its PSD child layers are kept and parented under the instance.
	static Control *build_scene(const Vector<PsdLayerInfo> &p_layers, const Size2i &p_doc_size, const Options &p_options, TextureMode p_mode, const HashMap<int, Ref<Texture2D>> *p_external_textures = nullptr, const HashMap<int, Ref<PackedScene>> *p_external_scenes = nullptr);

	static String sanitize_name(const String &p_name);

private:
	static Ref<Image> _extract_layer_image(void *p_document, void *p_layer);
	static Color _sample_dominant_color(const Ref<Image> &p_image);
};
