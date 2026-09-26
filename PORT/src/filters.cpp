#include "filters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <thread>

/* xBR interpolation adapted from Hyllian's xBR-lv2 shader:
 * https://github.com/libretro/glsl-shaders/blob/master/xbr/shaders/xbr-lv2.glsl
 * Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

namespace {
using Color = std::array<float,3>;
Color rgb(uint32_t pixel) { return {float((pixel>>16)&255),float((pixel>>8)&255),float(pixel&255)}; }
uint32_t pack(const Color& color) {
    uint32_t value=0xff000000;
    for (int c=0;c<3;++c) value|=uint32_t(std::clamp(int(color[c]+.5f),0,255))<<(16-c*8);
    return value;
}
Color mix(const Color& a,const Color& b,float weight) {
    return {a[0]+(b[0]-a[0])*weight,a[1]+(b[1]-a[1])*weight,a[2]+(b[2]-a[2])*weight};
}
float luma(uint32_t pixel) {
    const auto c=rgb(pixel); return (14.352f*c[0]+28.176f*c[1]+5.472f*c[2])/255.f;
}
float distance(const Color& a,const Color& b) {
    return std::abs(a[0]-b[0])+std::abs(a[1]-b[1])+std::abs(a[2]-b[2]);
}
struct Corner { int flags=0; Color color{}; };
using Weights=std::array<std::array<std::array<float,9>,4>,16>;
const Weights& xbr_weights() {
    static const Weights table=[] {
        Weights weights{};
        for (int flags=0;flags<16;++flags) for (int r=0;r<4;++r)
            for (int sy=0;sy<3;++sy) for (int sx=0;sx<3;++sx) {
                float px=(sx+.5f)/3.f,py=(sy+.5f)/3.f;
                for (int n=0;n<r;++n) {float previous=px;px=1-py;py=previous;}
                auto ramp=[](float value,float delta) {return std::clamp((value+delta)/(2*delta),0.f,1.f);};
                weights[flags][r][sy*3+sx]=std::max({flags&2 ? ramp(px+py-1.5f,1.f/3):0.f,
                    flags&1 ? ramp(px+py-1.75f,1.f/3):0.f,
                    flags&4 ? ramp(.5f*px+py-1.f,1.f/6):0.f,
                    flags&8 ? ramp(2*px+py-2.f,1.f/3):0.f});
            }
        return weights;
    }();
    return table;
}
}

int filter_scale(DisplayFilter filter) {
    return filter==DisplayFilter::Scale3x || filter==DisplayFilter::Xbr ? 3 :
           filter==DisplayFilter::Scale2x || filter==DisplayFilter::Crt ? 2 : 1;
}

std::vector<uint32_t> filter_pixels(const uint32_t* source,int width,int height,DisplayFilter filter) {
    if (!source || width<=0 || height<=0 || width>8192 || height>8192)
        throw std::invalid_argument("Invalid filter image");
    const int scale=filter_scale(filter), stride=width*scale;
    std::vector<uint32_t> out(size_t(stride)*height*scale);
    auto pixel=[&](int x,int y) {return source[std::clamp(y,0,height-1)*width+std::clamp(x,0,width-1)];};
    if (scale==1) {std::copy(source,source+size_t(width)*height,out.begin());return out;}
    // Share luminance and border expansion between all four corner tests.
    const int padded_width=width+4;
    std::vector<float> light;
    std::vector<uint32_t> padded;
    int offsets[4][11]{};
    const auto& weights=xbr_weights();
    if (filter==DisplayFilter::Xbr) {
        light.resize(size_t(padded_width)*(height+4));padded.resize(light.size());
        for (int y=-2;y<height+2;++y) for (int x=-2;x<width+2;++x) {
            const int index=(y+2)*padded_width+x+2;
            padded[index]=pixel(x,y);light[index]=luma(padded[index]);
        }
        const int neighbors[11][2]={{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1},{0,2},{2,0},{2,1},{1,2}};
        for (int r=0;r<4;++r) for (int n=0;n<11;++n) {
            int dx=neighbors[n][0],dy=neighbors[n][1];
            for (int rotation=0;rotation<r;++rotation) {int previous=dx;dx=dy;dy=-previous;}
            offsets[r][n]=dy*padded_width+dx;
        }
    }
    auto render_rows=[&](int begin,int end) {
    for (int y=begin;y<end;++y) for (int x=0;x<width;++x) {
        const uint32_t A=pixel(x-1,y-1),B=pixel(x,y-1),C=pixel(x+1,y-1),D=pixel(x-1,y),
                       E=pixel(x,y),F=pixel(x+1,y),G=pixel(x-1,y+1),H=pixel(x,y+1),I=pixel(x+1,y+1);
        if (filter==DisplayFilter::Scale2x || filter==DisplayFilter::Scale3x) {
            uint32_t block[9]; std::fill(block,block+9,E);
            if (B!=H && D!=F) {
                if (scale==2) {block[0]=D==B ? D:E;block[1]=B==F ? F:E;block[2]=D==H ? D:E;block[3]=H==F ? F:E;}
                else {
                    block[0]=D==B ? D:E; block[1]=(D==B && E!=C)||(B==F && E!=A) ? B:E; block[2]=B==F ? F:E;
                    block[3]=(D==B && E!=G)||(D==H && E!=A) ? D:E;
                    block[5]=(B==F && E!=I)||(H==F && E!=C) ? F:E;
                    block[6]=D==H ? D:E; block[7]=(D==H && E!=I)||(H==F && E!=G) ? H:E; block[8]=H==F ? F:E;
                }
            }
            for (int sy=0;sy<scale;++sy) for (int sx=0;sx<scale;++sx)
                out[(y*scale+sy)*stride+x*scale+sx]=block[sy*scale+sx];
        } else if (filter==DisplayFilter::Xbr) {
            std::array<Corner,4> corners;
            const int origin=(y+2)*padded_width+x+2;
            for (int rotation=0;rotation<4;++rotation) {
                const auto* neighbors=offsets[rotation];
                const float e=light[origin],b=light[origin+neighbors[0]],c=light[origin+neighbors[1]],d=light[origin+neighbors[2]],
                    f=light[origin+neighbors[3]],g=light[origin+neighbors[4]],h=light[origin+neighbors[5]],i=light[origin+neighbors[6]],
                    h5=light[origin+neighbors[7]],f4=light[origin+neighbors[8]],i4=light[origin+neighbors[9]],i5=light[origin+neighbors[10]];
                auto df=[](float a,float b) {return std::abs(a-b);};
                auto eq=[&](float a,float b) {return df(a,b)<=15.f;};
                const float wd1=df(e,c)+df(e,g)+df(i,h5)+df(i,f4)+4*df(h,f);
                const float wd2=df(h,d)+df(h,i5)+df(f,i4)+df(f,b)+4*df(e,i);
                const bool restrict=e!=f && e!=h;
                const bool detail=(!eq(f,b)&&!eq(f,c)) || (!eq(h,d)&&!eq(h,g)) ||
                    (eq(e,i)&&((!eq(f,f4)&&!eq(f,i4))||(!eq(h,h5)&&!eq(h,i5)))) || eq(e,g)||eq(e,c);
                auto& corner=corners[rotation];
                const bool edge=restrict && detail && wd1+.1f<=wd2;
                corner.flags=(restrict && wd1<=wd2 ? 1:0)|(edge ? 2:0)|
                    (edge && e!=g && d!=g && 2*df(f,g)<=df(h,c) ? 4:0)|
                    (edge && e!=c && b!=c && 2*df(h,c)<=df(f,g) ? 8:0);
                corner.color=rgb(padded[origin+neighbors[df(e,f)<=df(e,h) ? 3:5]]);
            }
            const Color center=rgb(E);
            for (int sy=0;sy<3;++sy) for (int sx=0;sx<3;++sx) {
                const int index=sy*3+sx;
                const auto first=mix(mix(center,corners[0].color,weights[corners[0].flags][0][index]),
                                     corners[2].color,weights[corners[2].flags][2][index]);
                const auto second=mix(mix(center,corners[1].color,weights[corners[1].flags][1][index]),
                                      corners[3].color,weights[corners[3].flags][3][index]);
                out[(y*3+sy)*stride+x*3+sx]=pack(distance(center,second)>=distance(center,first) ? second:first);
            }
        } else {
            // Soft CRT: bilinear reconstruction, mild scanlines and an RGB mask.
            for (int sy=0;sy<2;++sy) for (int sx=0;sx<2;++sx) {
                const int left=x+(sx==0 ? -1:0),top=y+(sy==0 ? -1:0);
                const float fx=sx==0 ? .75f:.25f,fy=sy==0 ? .75f:.25f;
                Color value=mix(mix(rgb(pixel(left,top)),rgb(pixel(left+1,top)),fx),
                                mix(rgb(pixel(left,top+1)),rgb(pixel(left+1,top+1)),fx),fy);
                for (int c=0;c<3;++c) value[c]*=(sy==0 ? 1.f:.84f)*((x*2+sx)%3==c ? 1.f:.96f);
                out[(y*2+sy)*stride+x*2+sx]=pack(value);
            }
        }
    }
    };
    const int threads=filter==DisplayFilter::Xbr && size_t(width)*height>=65536 ?
        std::min(4,int(std::max(1u,std::thread::hardware_concurrency()))) : 1;
    std::vector<std::thread> workers;
    for (int n=1;n<threads;++n) workers.emplace_back(render_rows,height*n/threads,height*(n+1)/threads);
    render_rows(0,height/threads);
    for (auto& worker : workers) worker.join();
    return out;
}
