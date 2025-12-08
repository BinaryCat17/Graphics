#ifndef ASSETS_H
#define ASSETS_H

typedef struct Assets {
    char* model_path;
    char* layout_path;
    char* styles_path;
    char* vert_spv_path;
    char* frag_spv_path;
    char* font_path;

    char* model_text;
    char* layout_text;
    char* styles_text;
} Assets;

int load_assets(const char* assets_dir, Assets* out_assets);
void free_assets(Assets* assets);

#endif // ASSETS_H
