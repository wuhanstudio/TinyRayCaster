#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <sstream>
#include <iomanip>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define CHANNEL_NUM 3

uint32_t pack_color(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a = 255) {
    return (a << 24) + (b << 16) + (g << 8) + r;
}

void unpack_color(const uint32_t& color, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    r = (color >> 0) & 255;
    g = (color >> 8) & 255;
    b = (color >> 16) & 255;
    a = (color >> 24) & 255;
}

bool load_texture(const std::string filename, std::vector<uint32_t>& texture, size_t& text_size, size_t& text_cnt) {
    int nchannels = -1, w, h;
    unsigned char* pixmap = stbi_load(filename.c_str(), &w, &h, &nchannels, 0);
    if (!pixmap) {
        std::cerr << "Error: can not load the textures" << std::endl;
        return false;
    }

    if (4 != nchannels) {
        std::cerr << "Error: the texture must be a 32 bit image" << std::endl;
        stbi_image_free(pixmap);
        return false;
    }

    text_cnt = w / h;
    text_size = w / text_cnt;
    if (w != h * int(text_cnt)) {
        std::cerr << "Error: the texture file must contain N square textures packed horizontally" << std::endl;
        stbi_image_free(pixmap);
        return false;
    }

    texture = std::vector<uint32_t>(w * h);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint8_t r = pixmap[(i + j * w) * 4 + 0];
            uint8_t g = pixmap[(i + j * w) * 4 + 1];
            uint8_t b = pixmap[(i + j * w) * 4 + 2];
            uint8_t a = pixmap[(i + j * w) * 4 + 3];
            texture[i + j * w] = pack_color(r, g, b, a);
        }
    }
    stbi_image_free(pixmap);
    return true;
}

std::vector<uint32_t> texture_column(const std::vector<uint32_t>& img, const size_t texsize, const size_t ntextures, const size_t texid, const size_t texcoord, const size_t column_height) {
    const size_t img_w = texsize * ntextures;
    const size_t img_h = texsize;
    assert(img.size() == img_w * img_h && texcoord < texsize&& texid < ntextures);
    std::vector<uint32_t> column(column_height);
    for (size_t y = 0; y < column_height; y++) {
        size_t pix_x = texid * texsize + texcoord;
        size_t pix_y = (y * texsize) / column_height;
        column[y] = img[pix_x + pix_y * img_w];
    }
    return column;
}

void draw_rectangle(uint8_t* pixels, const size_t img_w, const size_t img_h, const size_t x, const size_t y, const size_t w, const size_t h, const uint32_t color) {
    //assert(img.size() == img_w * img_h);
    uint8_t r, g, b, a;
    unpack_color(color, r, g, b, a);
    for (size_t i = 0; i < w; i++) {
        for (size_t j = 0; j < h; j++) {
            size_t cx = x + i;
            size_t cy = y + j;
            if (cx >= img_w || cy >= img_h) continue; // no need to check negative values, (unsigned variables)
            pixels[(cx + cy * img_w) * 3 + 0] = r;
            pixels[(cx + cy * img_w) * 3 + 1] = g;
            pixels[(cx + cy * img_w) * 3 + 2] = b;
        }
    }
}

const char* to_cstr(std::string&& s)
{
    static thread_local std::string sloc;
    sloc = std::move(s);
    return sloc.c_str();
}


int main() {
    const size_t win_w = 1024; // image width
    const size_t win_h = 512; // image height

    int index = 0;
    uint8_t* pixels = new uint8_t[win_w * win_h * CHANNEL_NUM];
    std::fill_n(pixels, win_w * win_h * CHANNEL_NUM, 255);

    const size_t map_w = 16; // map width
    const size_t map_h = 16; // map height
    const char map[] = \
        "0000222222220000"\
        "1              0"\
        "1      11111   0"\
        "1     0        0"\
        "0     0  1110000"\
        "0     3        0"\
        "0   10000      0"\
        "0   3   11100  0"\
        "5   4   0      0"\
        "5   4   1  00000"\
        "0       1      0"\
        "2       1      0"\
        "0       0      0"\
        "0 0000000      0"\
        "0              0"\
        "0002222222200000"; // our game map
    assert(sizeof(map) == map_w * map_h + 1); // +1 for the null terminated string

    float player_x = 3.456; // player x position
    float player_y = 2.345; // player y position
    float player_a = 1.523; // player view direction
    const float fov = M_PI / 3.; // field of view

    std::vector<uint32_t> walltext; // textures for the walls
    size_t walltext_size;  // texture dimensions (it is a square)
    size_t walltext_cnt;   // number of different textures in the image
    if (!load_texture("walltext.png", walltext, walltext_size, walltext_cnt)) {
        std::cerr << "Failed to load wall textures" << std::endl;
        return -1;
    }

    const size_t rect_w = win_w / (map_w * 2);
    const size_t rect_h = win_h / map_h;

    for (size_t frame = 0; frame < 360; frame++) {
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(5) << frame << ".jpg";
        printf("Frame %d\n", frame);
        player_a += 2 * M_PI / 360;

        std::fill_n(pixels, win_w * win_h * CHANNEL_NUM, 255);

        for (size_t j = 0; j < map_h; j++) { // draw the map
            for (size_t i = 0; i < map_w; i++) {
                if (map[i + j * map_w] == ' ') continue; // skip empty spaces
                size_t rect_x = i * rect_w;
                size_t rect_y = j * rect_h;
                size_t texid = map[i + j * map_w] - '0';
                assert(texid < walltext_cnt);
                draw_rectangle(pixels, win_w, win_h, rect_x, rect_y, rect_w, rect_h, walltext[texid * walltext_size]); // the color is taken from the upper left pixel of the texture #texid
            }
        }

        for (size_t i = 0; i < win_w / 2; i++) { // draw the visibility cone AND the "3D" view
            float angle = player_a - fov / 2 + fov * i / float(win_w / 2);
            for (float t = 0; t < 20; t += .01) {
                float cx = player_x + t * cos(angle);
                float cy = player_y + t * sin(angle);

                int pix_x = cx * rect_w;
                int pix_y = cy * rect_h;
                pixels[(pix_x + pix_y * win_w) * 3 + 0] = 160; // this draws the visibility cone
                pixels[(pix_x + pix_y * win_w) * 3 + 1] = 160; // this draws the visibility cone
                pixels[(pix_x + pix_y * win_w) * 3 + 2] = 160; // this draws the visibility cone

                if (map[int(cx) + int(cy) * map_w] != ' ') { // our ray touches a wall, so draw the vertical column to create an illusion of 3D
                    size_t texid = map[int(cx) + int(cy) * map_w] - '0';
                    assert(texid < walltext_cnt);
                    size_t column_height = win_h / (t * cos(angle - player_a));
                    float hitx = cx - floor(cx + .5); // hitx and hity contain (signed) fractional parts of cx and cy,
                    float hity = cy - floor(cy + .5); // they vary between -0.5 and +0.5, and one of them is supposed to be very close to 0
                    int x_texcoord = hitx * walltext_size;
                    if (std::abs(hity) > std::abs(hitx)) { // we need to determine whether we hit a "vertical" or a "horizontal" wall (w.r.t the map)
                        x_texcoord = hity * walltext_size;
                    }
                    if (x_texcoord < 0) x_texcoord += walltext_size; // do not forget x_texcoord can be negative, fix that
                    assert(x_texcoord >= 0 && x_texcoord < (int)walltext_size);

                    std::vector<uint32_t> column = texture_column(walltext, walltext_size, walltext_cnt, texid, x_texcoord, column_height);
                    pix_x = win_w / 2 + i;
                    for (size_t j = 0; j < column_height; j++) {
                        pix_y = j + win_h / 2 - column_height / 2;
                        if (pix_y < 0 || pix_y >= (int)win_h) continue;
                        uint8_t r, g, b, a;
                        unpack_color(column[j], r, g, b, a);
                        pixels[(pix_x + pix_y * win_w) * 3 + 0 ] = r;
                        pixels[(pix_x + pix_y * win_w) * 3 + 1 ] = g;
                        pixels[(pix_x + pix_y * win_w) * 3 + 2 ] = b;
                    }
                        break;
                }
            }
        }

        //const size_t texid = 4; // draw the 4th texture on the screen
        //for (size_t i = 0; i < walltext_size; i++) {
        //    for (size_t j = 0; j < walltext_size; j++) {
        //        uint8_t r, g, b, a;
        //        unpack_color(walltext[i + texid * walltext_size + j * walltext_size * walltext_cnt], r, g, b, a);
        //        pixels[(i + j * win_w) * 3 + 0] = r;
        //        pixels[(i + j * win_w) * 3 + 1] = g;
        //        pixels[(i + j * win_w) * 3 + 2] = b;
        //    }
        //}

        stbi_write_jpg(ss.str().c_str(), win_w, win_h, 3, pixels, 100);
    }

    //stbi_write_jpg("out.jpg", win_w, win_h, 3, pixels, 100);
    delete[] pixels;

    return 0;
}
