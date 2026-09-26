#include "settings.h"
#include "filters.h"
#include "roster.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdio>
#include <stdexcept>

static void check(bool ok,const char* message) {if (!ok) throw std::runtime_error(message);}
int main() {
    const auto path=std::filesystem::temp_directory_path() /
        ("rise2-options-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    try {
        Settings settings; settings.load(path.string());
        check(settings.music_volume==100 && settings.game_volume==100 && !settings.easy_finishings,"missing options must retain defaults");
        settings.music_volume=35;settings.game_volume=0;settings.easy_finishings=true;settings.filter=DisplayFilter::Xbr;
        check(settings.save(path.string()),"options must save");
        Settings loaded;loaded.load(path.string());
        check(loaded.music_volume==35 && loaded.game_volume==0 && loaded.easy_finishings && loaded.filter==DisplayFilter::Xbr,
              "volumes, mute, finishing assist and filter must survive a restart");
        {std::ofstream file(path);file<<R"({"music_volume":-10,"game_volume":150,"filter":100,"easy_finishings":"yes"})";}
        loaded.load(path.string());
        check(loaded.music_volume==0 && loaded.game_volume==100 && loaded.filter==DisplayFilter::Nearest && !loaded.easy_finishings,
              "out-of-range options must clamp or retain safe defaults");
        {std::ofstream file(path);file<<"broken JSON";}
        loaded.load(path.string());check(loaded.music_volume==100,"corrupt options must not prevent startup");

        const uint32_t solid[]={0xff384c80};
        for (auto filter : {DisplayFilter::Nearest,DisplayFilter::Bilinear,DisplayFilter::Scale2x,DisplayFilter::Scale3x,DisplayFilter::Xbr}) {
            const auto out=filter_pixels(solid,1,1,filter);
            for (auto pixel : out) check(pixel==solid[0],"flat colors and image borders must survive filtering");
        }
        const uint32_t black=0xff000000,white=0xffffffff;
        const uint32_t diagonal[]={white,black,black,black,white,black,black,black,white};
        auto scale2=filter_pixels(diagonal,3,3,DisplayFilter::Scale2x);
        check(scale2.size()==36 && scale2[2*6+1]==white && scale2[2*6]==black,
              "Scale2x must reconstruct diagonal edges instead of pixel doubling");
        auto scale3=filter_pixels(diagonal,3,3,DisplayFilter::Scale3x);
        check(scale3.size()==81 && scale3[4*9+4]==white && scale3[3*9+2]==white,"Scale3x must preserve centers and round diagonal corners");
        auto xbr=filter_pixels(diagonal,3,3,DisplayFilter::Xbr);
        bool interpolated=false;
        for (auto pixel : xbr) {check((pixel>>24)==255,"xBR must retain opaque compositing");interpolated|=pixel!=black && pixel!=white;}
        check(interpolated,"xBR must blend reconstructed diagonals");
        auto crt=filter_pixels(solid,1,1,DisplayFilter::Crt);
        check(crt.size()==4 && ((crt[2]>>16)&255)<((crt[0]>>16)&255),"CRT must use mild scanlines without dropping rows");
        check(kRoster[0].slot=='A' && std::string(kRoster[0].name)=="CYBORG" && kRoster[26].slot=='0' && portrait_index(26)==26,
              "robot names and portraits must follow the native ABC...01 order");
        std::filesystem::remove(path);
        puts("Saved options, muted channels, invalid input and all display filters checked.");
        return 0;
    } catch (const std::exception& e) {std::filesystem::remove(path);std::fprintf(stderr,"%s\n",e.what());return 1;}
}
