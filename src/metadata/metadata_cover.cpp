#include "metadata.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tfile.h>
#include <taglib/audioproperties.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>

#include <cstdlib>
#include <cstring>

extern "C" bool music_metadata_read_cover(const char *file_path, music_cover_art_t *out_cover)
{
    if (!file_path || !out_cover) return false;
    out_cover->data = nullptr;
    out_cover->size = 0;
    out_cover->mime_type[0] = '\0';

    // 1. MPEG ID3v2 APIC frame
    try {
        TagLib::MPEG::File mpeg_file(file_path);
        if (mpeg_file.isValid() && mpeg_file.ID3v2Tag()) {
            const auto &frames = mpeg_file.ID3v2Tag()->frameListMap()["APIC"];
            if (!frames.isEmpty()) {
                auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
                if (pic && pic->picture().size() > 0) {
                    out_cover->size = pic->picture().size();
                    out_cover->data = static_cast<uint8_t *>(std::malloc(out_cover->size));
                    if (out_cover->data) {
                        std::memcpy(out_cover->data, pic->picture().data(), out_cover->size);
                        std::strncpy(out_cover->mime_type, pic->mimeType().toCString(true), sizeof(out_cover->mime_type) - 1);
                        return true;
                    }
                }
            }
        }
    } catch (...) {}

    // 2. FLAC picture
    try {
        TagLib::FLAC::File flac_file(file_path);
        if (flac_file.isValid()) {
            const auto &pics = flac_file.pictureList();
            if (!pics.isEmpty()) {
                auto *pic = pics.front();
                if (pic && pic->data().size() > 0) {
                    out_cover->size = pic->data().size();
                    out_cover->data = static_cast<uint8_t *>(std::malloc(out_cover->size));
                    if (out_cover->data) {
                        std::memcpy(out_cover->data, pic->data().data(), out_cover->size);
                        std::strncpy(out_cover->mime_type, pic->mimeType().toCString(true), sizeof(out_cover->mime_type) - 1);
                        return true;
                    }
                }
            }
        }
    } catch (...) {}

    // 3. MP4 covr item
    try {
        TagLib::MP4::File mp4_file(file_path);
        if (mp4_file.isValid() && mp4_file.tag()) {
            const auto &items = mp4_file.tag()->itemMap();
            if (items.contains("covr")) {
                TagLib::MP4::CoverArtList art_list = items["covr"].toCoverArtList();
                if (!art_list.isEmpty()) {
                    const auto &art = art_list.front();
                    if (art.data().size() > 0) {
                        out_cover->size = art.data().size();
                        out_cover->data = static_cast<uint8_t *>(std::malloc(out_cover->size));
                        if (out_cover->data) {
                            std::memcpy(out_cover->data, art.data().data(), out_cover->size);
                            const char *mime = (art.format() == TagLib::MP4::CoverArt::PNG) ? "image/png" : "image/jpeg";
                            std::strncpy(out_cover->mime_type, mime, sizeof(out_cover->mime_type) - 1);
                            return true;
                        }
                    }
                }
            }
        }
    } catch (...) {}

    return false;
}
