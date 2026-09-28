#include "BWF.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <queue>
#include <utility>

static const PF_FpLong PI = 3.14159265358979323846;

static inline PF_FpLong clampF(PF_FpLong val, PF_FpLong minVal, PF_FpLong maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

static inline PF_FpLong lerpF(PF_FpLong a, PF_FpLong b, PF_FpLong t) {
    return a + (b - a) * t;
}

static inline A_long clampL(A_long val, A_long minVal, A_long maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

static inline PF_Pixel8* getPixel8(PF_LayerDef* layer, A_long x, A_long y) {
    A_long w = layer->extent_hint.right - layer->extent_hint.left;
    x = clampL(x, 0, w - 1);
    y = clampL(y, 0, layer->extent_hint.bottom - layer->extent_hint.top - 1);
    return (PF_Pixel8*)((char*)layer->data + y * layer->rowbytes) + x;
}

static inline PF_Pixel16* getPixel16(PF_LayerDef* layer, A_long x, A_long y) {
    A_long w = layer->extent_hint.right - layer->extent_hint.left;
    x = clampL(x, 0, w - 1);
    y = clampL(y, 0, layer->extent_hint.bottom - layer->extent_hint.top - 1);
    return (PF_Pixel16*)((char*)layer->data + y * layer->rowbytes) + x;
}

static inline PF_FpLong luminance8(PF_Pixel8* p) {
    return 0.299 * p->red + 0.587 * p->green + 0.114 * p->blue;
}

static inline PF_FpLong luminance16(PF_Pixel16* p) {
    return 0.299 * (p->red / 65535.0) + 0.587 * (p->green / 65535.0) + 0.114 * (p->blue / 65535.0);
}

static PF_FpLong sampleLumBilinear(const std::vector<PF_FpLong>& buf,
    PF_FpLong x, PF_FpLong y, A_long w, A_long h) {
    PF_FpLong fx = clampF(x, 0.0, (PF_FpLong)(w - 1));
    PF_FpLong fy = clampF(y, 0.0, (PF_FpLong)(h - 1));
    A_long ix0 = (A_long)std::floor(fx);
    A_long iy0 = (A_long)std::floor(fy);
    A_long ix1 = clampL(ix0 + 1, 0, w - 1);
    A_long iy1 = clampL(iy0 + 1, 0, h - 1);
    PF_FpLong tx = fx - (PF_FpLong)ix0;
    PF_FpLong ty = fy - (PF_FpLong)iy0;
    PF_FpLong v00 = buf[iy0 * w + ix0];
    PF_FpLong v10 = buf[iy0 * w + ix1];
    PF_FpLong v01 = buf[iy1 * w + ix0];
    PF_FpLong v11 = buf[iy1 * w + ix1];
    return lerpF(lerpF(v00, v10, tx), lerpF(v01, v11, tx), ty);
}

struct Point {
    A_long x, y;
    Point() : x(0), y(0) {}
    Point(A_long _x, A_long _y) : x(_x), y(_y) {}
};

static PF_FpLong pointLineDist(const Point& p, const Point& a, const Point& b) {
    PF_FpLong dx = (PF_FpLong)(b.x - a.x);
    PF_FpLong dy = (PF_FpLong)(b.y - a.y);
    PF_FpLong lenSq = dx * dx + dy * dy;
    if (lenSq < 0.001) {
        PF_FpLong dpx = (PF_FpLong)(p.x - a.x);
        PF_FpLong dpy = (PF_FpLong)(p.y - a.y);
        return std::sqrt(dpx * dpx + dpy * dpy);
    }
    PF_FpLong t = ((PF_FpLong)(p.x - a.x) * dx + (PF_FpLong)(p.y - a.y) * dy) / lenSq;
    t = clampF(t, 0.0, 1.0);
    PF_FpLong projX = (PF_FpLong)a.x + t * dx;
    PF_FpLong projY = (PF_FpLong)a.y + t * dy;
    PF_FpLong dpx = (PF_FpLong)p.x - projX;
    PF_FpLong dpy = (PF_FpLong)p.y - projY;
    return std::sqrt(dpx * dpx + dpy * dpy);
}

static void douglasPeuckerRecursive(std::vector<Point>& pts, A_long start, A_long end,
    PF_FpLong epsilon, std::vector<bool>& keep) {
    if (end - start <= 1) return;
    PF_FpLong maxDist = 0.0;
    A_long maxIdx = start;
    for (A_long i = start + 1; i < end; i++) {
        PF_FpLong d = pointLineDist(pts[i], pts[start], pts[end]);
        if (d > maxDist) {
            maxDist = d;
            maxIdx = i;
        }
    }
    if (maxDist > epsilon) {
        keep[maxIdx] = true;
        douglasPeuckerRecursive(pts, start, maxIdx, epsilon, keep);
        douglasPeuckerRecursive(pts, maxIdx, end, epsilon, keep);
    }
}

static void douglasPeucker(std::vector<Point>& pts, PF_FpLong epsilon) {
    if (pts.size() <= 2) return;
    std::vector<bool> keep(pts.size(), false);
    keep[0] = true;
    keep[pts.size() - 1] = true;
    douglasPeuckerRecursive(pts, 0, (A_long)pts.size() - 1, epsilon, keep);
    std::vector<Point> result;
    result.reserve(pts.size());
    for (size_t i = 0; i < pts.size(); i++) {
        if (keep[i]) result.push_back(pts[i]);
    }
    pts = std::move(result);
}

static void stage1Threshold8(PF_LayerDef* input,
    std::vector<PF_FpLong>& buf,
    PF_FpLong threshold, PF_FpLong contrast,
    A_long w, A_long h) {
    for (A_long y = 0; y < h; y++) {
        for (A_long x = 0; x < w; x++) {
            PF_FpLong lum = luminance8(getPixel8(input, x, y));
            PF_FpLong cl = (lum - 128.0) * contrast + 128.0;
            cl = clampF(cl, 0.0, 255.0);
            buf[y * w + x] = (cl > threshold) ? 1.0 : 0.0;
        }
    }
}

static void stage1Threshold16(PF_LayerDef* input,
    std::vector<PF_FpLong>& buf,
    PF_FpLong threshold01, PF_FpLong contrast,
    A_long w, A_long h) {
    for (A_long y = 0; y < h; y++) {
        for (A_long x = 0; x < w; x++) {
            PF_FpLong lum = luminance16(getPixel16(input, x, y));
            PF_FpLong cl = (lum - 0.5) * contrast + 0.5;
            cl = clampF(cl, 0.0, 1.0);
            buf[y * w + x] = (cl > threshold01) ? 1.0 : 0.0;
        }
    }
}

static bool isEdge(const std::vector<PF_FpLong>& binary,
    A_long x, A_long y, A_long w, A_long h) {
    PF_FpLong v = binary[y * w + x];
    for (A_long dy = -1; dy <= 1; dy++) {
        for (A_long dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            A_long nx = x + dx;
            A_long ny = y + dy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (binary[ny * w + nx] != v) return true;
        }
    }
    return false;
}

static void stage2ComputeSDF(const std::vector<PF_FpLong>& binary,
    std::vector<PF_FpLong>& sdf,
    A_long w, A_long h) {
    const PF_FpLong INF = (PF_FpLong)(w + h) * 2.0;
    sdf.resize(w * h);

    for (A_long y = 0; y < h; y++) {
        for (A_long x = 0; x < w; x++) {
            if (isEdge(binary, x, y, w, h)) {
                sdf[y * w + x] = 0.0;
            } else if (binary[y * w + x] > 0.5) {
                sdf[y * w + x] = INF;
            } else {
                sdf[y * w + x] = -INF;
            }
        }
    }

    for (A_long y = 2; y < h - 2; y++) {
        for (A_long x = 2; x < w - 2; x++) {
            PF_FpLong v = sdf[y * w + x];
            PF_FpLong absV = std::abs(v);
            if (absV < 0.001) continue;

            PF_FpLong best = absV;
            for (A_long dy = -2; dy <= 2; dy++) {
                for (A_long dx = -2; dx <= 2; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    PF_FpLong nv = sdf[(y + dy) * w + (x + dx)];
                    PF_FpLong nd;
                    if (nv >= 0.0) {
                        nd = nv + std::sqrt((PF_FpLong)(dx * dx + dy * dy));
                    } else {
                        nd = nv - std::sqrt((PF_FpLong)(dx * dx + dy * dy));
                    }
                    PF_FpLong cand = (v >= 0.0) ? std::abs(nd) : -std::abs(nd);
                    cand = (v >= 0.0) ? std::max(cand, 0.0) : std::min(cand, 0.0);
                    PF_FpLong absCand = std::abs(cand);
                    if (absCand < best) {
                        best = absCand;
                        sdf[y * w + x] = cand;
                    }
                }
            }
        }
    }

    for (A_long y = h - 3; y >= 2; y--) {
        for (A_long x = w - 3; x >= 2; x--) {
            PF_FpLong v = sdf[y * w + x];
            PF_FpLong absV = std::abs(v);
            if (absV < 0.001) continue;

            PF_FpLong best = absV;
            for (A_long dy = -2; dy <= 2; dy++) {
                for (A_long dx = -2; dx <= 2; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    PF_FpLong nv = sdf[(y + dy) * w + (x + dx)];
                    PF_FpLong nd;
                    if (nv >= 0.0) {
                        nd = nv + std::sqrt((PF_FpLong)(dx * dx + dy * dy));
                    } else {
                        nd = nv - std::sqrt((PF_FpLong)(dx * dx + dy * dy));
                    }
                    PF_FpLong cand = (v >= 0.0) ? std::abs(nd) : -std::abs(nd);
                    cand = (v >= 0.0) ? std::max(cand, 0.0) : std::min(cand, 0.0);
                    PF_FpLong absCand = std::abs(cand);
                    if (absCand < best) {
                        best = absCand;
                        sdf[y * w + x] = cand;
                    }
                }
            }
        }
    }
}

static void computeSDFGradient(const std::vector<PF_FpLong>& sdf,
    A_long x, A_long y, A_long w, A_long h,
    PF_FpLong& gx, PF_FpLong& gy) {
    PF_FpLong xm1 = sdf[y * w + clampL(x - 1, 0, w - 1)];
    PF_FpLong xp1 = sdf[y * w + clampL(x + 1, 0, w - 1)];
    PF_FpLong ym1 = sdf[clampL(y - 1, 0, h - 1) * w + x];
    PF_FpLong yp1 = sdf[clampL(y + 1, 0, h - 1) * w + x];
    gx = (xp1 - xm1) * 0.5;
    gy = (yp1 - ym1) * 0.5;
}

static void traceContour(const std::vector<PF_FpLong>& binary,
    std::vector<unsigned char>& visited,
    A_long w, A_long h,
    A_long startX, A_long startY,
    std::vector<Point>& contour) {
    std::vector<Point> stack;
    stack.reserve(w * h);
    stack.push_back(Point(startX, startY));
    visited[startY * w + startX] = 1;

    contour.reserve(4096);
    while (!stack.empty()) {
        Point p = stack.back();
        stack.pop_back();
        contour.push_back(p);
        for (A_long dy = -1; dy <= 1; dy++) {
            for (A_long dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                A_long nx = p.x + dx;
                A_long ny = p.y + dy;
                if (nx < 1 || nx >= w - 1 || ny < 1 || ny >= h - 1) continue;
                A_long nidx = ny * w + nx;
                if (visited[nidx]) continue;
                if (isEdge(binary, nx, ny, w, h)) {
                    visited[nidx] = 1;
                    stack.push_back(Point(nx, ny));
                }
            }
        }
    }
}

static void extractSimplifiedContours(const std::vector<PF_FpLong>& binary,
    PF_FpLong simplifyTolerance,
    A_long w, A_long h,
    std::vector<Point>& outContourPoints) {
    outContourPoints.clear();

    std::vector<unsigned char> visited(w * h, 0);

    for (A_long y = 2; y < h - 2; y++) {
        for (A_long x = 2; x < w - 2; x++) {
            A_long idx = y * w + x;
            if (visited[idx]) continue;
            if (!isEdge(binary, x, y, w, h)) continue;
            if (binary[idx] < 0.5) continue;

            std::vector<Point> contour;
            traceContour(binary, visited, w, h, x, y, contour);

            if (contour.size() < 3) {
                for (auto& pt : contour) outContourPoints.push_back(pt);
                continue;
            }

            if (simplifyTolerance > 0.5) {
                douglasPeucker(contour, simplifyTolerance);
            }

            for (auto& pt : contour) outContourPoints.push_back(pt);
        }
    }
}

static void stage4NormalEmission(const std::vector<PF_FpLong>& sdf,
    std::vector<PF_FpLong>& lightMap,
    const std::vector<Point>& contourPoints,
    A_long sampleSpacing,
    PF_FpLong lightIntensity, PF_FpLong lightLength,
    PF_FpLong edgeIntensity,
    A_long w, A_long h) {
    if (lightIntensity < 0.001 || contourPoints.empty()) return;
    if (sampleSpacing < 1) sampleSpacing = 1;

    PF_FpLong stepLen = 2.0;
    A_long maxSteps = (A_long)(lightLength / stepLen) + 1;
    maxSteps = clampL(maxSteps, 1, 300);

    for (size_t i = 0; i < contourPoints.size(); i += (size_t)sampleSpacing) {
        const Point& cp = contourPoints[i];

        PF_FpLong gx = 0.0, gy = 0.0;
        computeSDFGradient(sdf, cp.x, cp.y, w, h, gx, gy);
        PF_FpLong gradMag = std::sqrt(gx * gx + gy * gy);
        if (gradMag < 0.0001) continue;

        PF_FpLong nx = -gx / gradMag;
        PF_FpLong ny = -gy / gradMag;

        if (sdf[cp.y * w + cp.x] < 0.0) {
            nx = -nx;
            ny = -ny;
        }

        PF_FpLong baseIntensity = lightIntensity * edgeIntensity;

        for (A_long s = 1; s <= maxSteps; s++) {
            PF_FpLong dist = (PF_FpLong)s * stepLen;
            PF_FpLong px = (PF_FpLong)cp.x + nx * dist;
            PF_FpLong py = (PF_FpLong)cp.y + ny * dist;
            A_long ix = (A_long)std::round(px);
            A_long iy = (A_long)std::round(py);
            if (ix < 0 || ix >= w || iy < 0 || iy >= h) break;

            PF_FpLong falloff = 1.0 - dist / (lightLength + 1.0);
            if (falloff <= 0.0) break;
            falloff = falloff * falloff;

            PF_FpLong thickness = 1.0 + (1.0 - falloff) * 2.0;
            A_long tR = (A_long)std::ceil(thickness);
            PF_FpLong intensity = falloff * baseIntensity;

            for (A_long dy = -tR; dy <= tR; dy++) {
                for (A_long dx = -tR; dx <= tR; dx++) {
                    A_long sx = ix + dx;
                    A_long sy = iy + dy;
                    if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
                    PF_FpLong d = std::sqrt((PF_FpLong)(dx * dx + dy * dy));
                    if (d > thickness) continue;
                    PF_FpLong soft = 1.0 - d / (thickness + 0.001);
                    PF_FpLong val = intensity * soft * soft;
                    A_long sidx = sy * w + sx;
                    if (val > lightMap[sidx]) lightMap[sidx] = val;
                }
            }
        }
    }
}

static void stage5RadialBlur(const std::vector<PF_FpLong>& src,
    std::vector<PF_FpLong>& dst,
    PF_FpLong bcx, PF_FpLong bcy,
    PF_FpLong blurStrength, A_long blurQuality,
    A_long w, A_long h) {
    if (blurStrength < 0.001) {
        dst = src;
        return;
    }

    A_long samples = blurQuality * 2 + 1;

    for (A_long y = 0; y < h; y++) {
        for (A_long x = 0; x < w; x++) {
            PF_FpLong dx = (PF_FpLong)x - bcx;
            PF_FpLong dy = (PF_FpLong)y - bcy;
            PF_FpLong pixelDist = std::sqrt(dx * dx + dy * dy);
            if (pixelDist < 0.001) pixelDist = 0.001;

            PF_FpLong dirX = dx / pixelDist;
            PF_FpLong dirY = dy / pixelDist;

            PF_FpLong blurRange = blurStrength * pixelDist * 0.5;
            if (blurRange < 1.0) blurRange = 1.0;

            PF_FpLong acc = 0.0;
            PF_FpLong weightSum = 0.0;
            A_long half = (samples - 1) / 2;

            for (A_long i = -half; i <= half; i++) {
                PF_FpLong t = (PF_FpLong)i / (PF_FpLong)half;
                PF_FpLong offset = t * blurRange;
                PF_FpLong sx = (PF_FpLong)x + dirX * offset;
                PF_FpLong sy = (PF_FpLong)y + dirY * offset;
                PF_FpLong sw = 1.0 - std::abs(t);
                acc += sampleLumBilinear(src, sx, sy, w, h) * sw;
                weightSum += sw;
            }

            if (weightSum > 0.0) acc /= weightSum;
            dst[y * w + x] = acc;
        }
    }
}

static PF_Err About(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
        "BWF v%d.%d\rPipeline: Threshold > SDF > ContourDP > NormalEmission > RadialBlur",
        MAJOR_VERSION, MINOR_VERSION);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output) {
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION,
        BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output) {
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER("阈值", 0, 255, 0, 255, 34, THRESHOLD_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER("对比度", 50, 300, 50, 300, 0, 142,
        PF_Precision_HUNDREDTHS, 0, 0, CONTRAST_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER("边缘强度", 0, 100, 0, 100, 0, 50,
        PF_Precision_HUNDREDTHS, 0, 0, EDGE_INTENSITY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POINT("光线/模糊中心",
        0,
        0,
        0, CENTER_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER("光线强度", 0, 100, 0, 100, 0, 80,
        PF_Precision_HUNDREDTHS, 0, 0, LIGHT_INTENSITY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER("光线长度", 1, 300, 1, 300, 196, LIGHT_LENGTH_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER("轮廓简化", 0, 50, 0, 50, 0, 1,
        PF_Precision_HUNDREDTHS, 0, 0, CONTOUR_SIMPLIFY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER("法线采样密度", 1, 20, 1, 20, 1, LINE_DENSITY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER("模糊强度", 0, 100, 0, 100, 0, 15,
        PF_Precision_HUNDREDTHS, 0, 0, BLUR_STRENGTH_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER("模糊品质", 2, 16, 2, 16, 15, BLUR_QUALITY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER("闪光强度", 0, 100, 0, 100, 0, 100,
        PF_Precision_HUNDREDTHS, 0, 0, FLASH_INTENSITY_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("闪光颜色", 255, 255, 255, FLASH_COLOR_DISK_ID);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("背景颜色", 0, 0, 0, BG_COLOR_DISK_ID);

    out_data->num_params = BWF_NUM_PARAMS;
    return err;
}

static PF_Err Render8(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output) {
    PF_Err err = PF_Err_NONE;

    PF_LayerDef* input = &params[BWF_INPUT]->u.ld;
    A_long w = input->extent_hint.right - input->extent_hint.left;
    A_long h = input->extent_hint.bottom - input->extent_hint.top;
    A_long total = w * h;

    PF_FpLong threshold = (PF_FpLong)params[BWF_THRESHOLD]->u.sd.value;
    PF_FpLong contrast = params[BWF_CONTRAST]->u.fs_d.value / 100.0;
    PF_FpLong edgeIntensity = params[BWF_EDGE_INTENSITY]->u.fs_d.value / 100.0;
    PF_FpLong centerX = FIX_2_FLOAT(params[BWF_CENTER]->u.td.x_value);
    PF_FpLong centerY = FIX_2_FLOAT(params[BWF_CENTER]->u.td.y_value);
    PF_FpLong lightIntensity = params[BWF_LIGHT_INTENSITY]->u.fs_d.value / 100.0;
    PF_FpLong lightLength = (PF_FpLong)params[BWF_LIGHT_LENGTH]->u.sd.value;
    PF_FpLong contourSimplify = params[BWF_CONTOUR_SIMPLIFY]->u.fs_d.value;
    A_long lineDensity = params[BWF_LINE_DENSITY]->u.sd.value;
    PF_FpLong blurStrength = params[BWF_BLUR_STRENGTH]->u.fs_d.value / 100.0;
    A_long blurQuality = params[BWF_BLUR_QUALITY]->u.sd.value;
    PF_FpLong flashIntensity = params[BWF_FLASH_INTENSITY]->u.fs_d.value / 100.0;
    PF_Pixel8 flashColor = params[BWF_FLASH_COLOR]->u.cd.value;
    PF_Pixel8 bgColor = params[BWF_BG_COLOR]->u.cd.value;

    std::vector<PF_FpLong> binaryMap(total, 0.0);
    std::vector<PF_FpLong> sdf;
    std::vector<Point> contourPoints;
    std::vector<PF_FpLong> lightMap(total, 0.0);
    std::vector<PF_FpLong> blurred(total, 0.0);

    stage1Threshold8(input, binaryMap, threshold, contrast, w, h);

    stage2ComputeSDF(binaryMap, sdf, w, h);

    extractSimplifiedContours(binaryMap, contourSimplify, w, h, contourPoints);

    stage4NormalEmission(sdf, lightMap, contourPoints,
        lineDensity, lightIntensity, lightLength, edgeIntensity, w, h);

    for (A_long i = 0; i < total; i++) {
        binaryMap[i] = clampF(binaryMap[i] + lightMap[i], 0.0, 1.0);
    }

    stage5RadialBlur(binaryMap, blurred, centerX, centerY,
        blurStrength, blurQuality, w, h);

    for (A_long y = 0; y < h; y++) {
        PF_Pixel8* outRow = (PF_Pixel8*)((char*)output->data + y * output->rowbytes);
        for (A_long x = 0; x < w; x++) {
            A_long idx = y * w + x;
            PF_FpLong v = blurred[idx];
            v = clampF(v, 0.0, 1.0);

            PF_Pixel8* srcP = getPixel8(input, x, y);
            PF_FpLong outR = lerpF((PF_FpLong)bgColor.red, (PF_FpLong)flashColor.red, v);
            PF_FpLong outG = lerpF((PF_FpLong)bgColor.green, (PF_FpLong)flashColor.green, v);
            PF_FpLong outB = lerpF((PF_FpLong)bgColor.blue, (PF_FpLong)flashColor.blue, v);

            outR = lerpF((PF_FpLong)srcP->red, outR, flashIntensity);
            outG = lerpF((PF_FpLong)srcP->green, outG, flashIntensity);
            outB = lerpF((PF_FpLong)srcP->blue, outB, flashIntensity);

            outRow[x].alpha = srcP->alpha;
            outRow[x].red = (A_u_char)clampF(outR, 0.0, 255.0);
            outRow[x].green = (A_u_char)clampF(outG, 0.0, 255.0);
            outRow[x].blue = (A_u_char)clampF(outB, 0.0, 255.0);
        }
    }

    return err;
}

static PF_Err Render16(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output) {
    PF_Err err = PF_Err_NONE;

    PF_LayerDef* input = &params[BWF_INPUT]->u.ld;
    A_long w = input->extent_hint.right - input->extent_hint.left;
    A_long h = input->extent_hint.bottom - input->extent_hint.top;
    A_long total = w * h;

    PF_FpLong threshold01 = params[BWF_THRESHOLD]->u.sd.value / 255.0;
    PF_FpLong contrast = params[BWF_CONTRAST]->u.fs_d.value / 100.0;
    PF_FpLong edgeIntensity = params[BWF_EDGE_INTENSITY]->u.fs_d.value / 100.0;
    PF_FpLong centerX = FIX_2_FLOAT(params[BWF_CENTER]->u.td.x_value);
    PF_FpLong centerY = FIX_2_FLOAT(params[BWF_CENTER]->u.td.y_value);
    PF_FpLong lightIntensity = params[BWF_LIGHT_INTENSITY]->u.fs_d.value / 100.0;
    PF_FpLong lightLength = (PF_FpLong)params[BWF_LIGHT_LENGTH]->u.sd.value;
    PF_FpLong contourSimplify = params[BWF_CONTOUR_SIMPLIFY]->u.fs_d.value;
    A_long lineDensity = params[BWF_LINE_DENSITY]->u.sd.value;
    PF_FpLong blurStrength = params[BWF_BLUR_STRENGTH]->u.fs_d.value / 100.0;
    A_long blurQuality = params[BWF_BLUR_QUALITY]->u.sd.value;
    PF_FpLong flashIntensity = params[BWF_FLASH_INTENSITY]->u.fs_d.value / 100.0;
    PF_Pixel8 flashColor8 = params[BWF_FLASH_COLOR]->u.cd.value;
    PF_Pixel8 bgColor8 = params[BWF_BG_COLOR]->u.cd.value;

    std::vector<PF_FpLong> binaryMap(total, 0.0);
    std::vector<PF_FpLong> sdf;
    std::vector<Point> contourPoints;
    std::vector<PF_FpLong> lightMap(total, 0.0);
    std::vector<PF_FpLong> blurred(total, 0.0);

    stage1Threshold16(input, binaryMap, threshold01, contrast, w, h);

    stage2ComputeSDF(binaryMap, sdf, w, h);

    extractSimplifiedContours(binaryMap, contourSimplify, w, h, contourPoints);

    stage4NormalEmission(sdf, lightMap, contourPoints,
        lineDensity, lightIntensity, lightLength, edgeIntensity, w, h);

    for (A_long i = 0; i < total; i++) {
        binaryMap[i] = clampF(binaryMap[i] + lightMap[i], 0.0, 1.0);
    }

    stage5RadialBlur(binaryMap, blurred, centerX, centerY,
        blurStrength, blurQuality, w, h);

    PF_FpLong fcR = flashColor8.red / 255.0;
    PF_FpLong fcG = flashColor8.green / 255.0;
    PF_FpLong fcB = flashColor8.blue / 255.0;
    PF_FpLong bgR = bgColor8.red / 255.0;
    PF_FpLong bgG = bgColor8.green / 255.0;
    PF_FpLong bgB = bgColor8.blue / 255.0;

    for (A_long y = 0; y < h; y++) {
        PF_Pixel16* outRow = (PF_Pixel16*)((char*)output->data + y * output->rowbytes);
        for (A_long x = 0; x < w; x++) {
            A_long idx = y * w + x;
            PF_FpLong v = blurred[idx];
            v = clampF(v, 0.0, 1.0);

            PF_Pixel16* srcP = getPixel16(input, x, y);
            PF_FpLong outR = lerpF(bgR, fcR, v);
            PF_FpLong outG = lerpF(bgG, fcG, v);
            PF_FpLong outB = lerpF(bgB, fcB, v);

            outR = lerpF(srcP->red / 65535.0, outR, flashIntensity);
            outG = lerpF(srcP->green / 65535.0, outG, flashIntensity);
            outB = lerpF(srcP->blue / 65535.0, outB, flashIntensity);

            outRow[x].alpha = srcP->alpha;
            outRow[x].red = (A_u_short)clampF(outR * 65535.0, 0.0, 65535.0);
            outRow[x].green = (A_u_short)clampF(outG * 65535.0, 0.0, 65535.0);
            outRow[x].blue = (A_u_short)clampF(outB * 65535.0, 0.0, 65535.0);
        }
    }

    return err;
}

static PF_Err Render(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output) {
    PF_Err err = PF_Err_NONE;

    if (PF_WORLD_IS_DEEP(output)) {
        err = Render16(in_data, out_data, params, output);
    } else {
        err = Render8(in_data, out_data, params, output);
    }

    return err;
}

extern "C" DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite* inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion) {
    PF_Err result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        "BWF",
        "ADBE BWF",
        "Sample Plug-ins",
        AE_RESERVED_INFO,
        "EffectMain",
        "https://www.adobe.com"
    );
    return result;
}

PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data,
    PF_OutData* out_data, PF_ParamDef* params[],
    PF_LayerDef* output, void* extra) {
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                err = About(in_data, out_data, params, output);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_data, out_data, params, output);
                break;
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_data, out_data, params, output);
                break;
            case PF_Cmd_RENDER:
                err = Render(in_data, out_data, params, output);
                break;
        }
    } catch (PF_Err& thrown_err) {
        err = thrown_err;
    }
    return err;
}