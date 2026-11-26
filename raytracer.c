/* raytracer.c — motor de raytracing desde cero, sin dependencias.
 * gcc -O3 -pthread raytracer.c -o ray -lm && ./ray > scene.ppm
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>

typedef struct { double x, y, z; } Vec;

static Vec vadd(Vec a, Vec b) { return (Vec){a.x+b.x, a.y+b.y, a.z+b.z}; }
static Vec vsub(Vec a, Vec b) { return (Vec){a.x-b.x, a.y-b.y, a.z-b.z}; }
static Vec vmul(Vec a, Vec b) { return (Vec){a.x*b.x, a.y*b.y, a.z*b.z}; }
static Vec vscale(Vec a, double s) { return (Vec){a.x*s, a.y*s, a.z*s}; }
static double vdot(Vec a, Vec b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static double vlen(Vec a) { return sqrt(vdot(a,a)); }
static Vec vnorm(Vec a) { return vscale(a, 1.0/vlen(a)); }
static Vec vlerp(Vec a, Vec b, double t) { return vadd(vscale(a,1-t), vscale(b,t)); }
static double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

typedef struct { Vec pos, color; double intensity; } Light;

typedef struct {
    Vec center, color;
    double radius;
    double reflect;    /* 0 = difusa, 1 = espejo */
    double emissive;   /* luz propia */
} Sphere;

typedef struct {
    Sphere *spheres; int nspheres;
    Light *lights;   int nlights;
    int width, height, samples;
    Vec cam_pos, cam_look;
    double fov;
    uint8_t *pixels;
} Scene;

static const Vec CHECK_A = {0.95, 0.55, 0.30};
static const Vec CHECK_B = {0.12, 0.12, 0.15};

static double intersect_sphere(const Sphere *s, Vec o, Vec d) {
    Vec oc = vsub(o, s->center);
    double b = vdot(oc, d);
    double c = vdot(oc, oc) - s->radius*s->radius;
    double disc = b*b - c;
    if (disc < 0) return -1;
    double sq = sqrt(disc), t = -b - sq;
    if (t > 1e-4) return t;
    t = -b + sq;
    return (t > 1e-4) ? t : -1;
}

static double intersect_floor(Vec o, Vec d) {
    if (fabs(d.y) < 1e-9) return -1;
    double t = -o.y / d.y;
    return (t > 1e-4) ? t : -1;
}

static Vec trace(const Scene *sc, Vec orig, Vec dir, int depth) {
    double best_t = 1e30;
    int hit_idx = -1, floor_hit = 0;

    for (int i = 0; i < sc->nspheres; i++) {
        double t = intersect_sphere(&sc->spheres[i], orig, dir);
        if (t > 0 && t < best_t) { best_t = t; hit_idx = i; floor_hit = 0; }
    }
    double ft = intersect_floor(orig, dir);
    if (ft > 0 && ft < best_t) { best_t = ft; hit_idx = -1; floor_hit = 1; }

    if (hit_idx < 0 && !floor_hit) {
        double t = 0.5 * (dir.y + 1.0);
        return vscale(vlerp((Vec){0.68,0.52,0.42}, (Vec){0.05,0.10,0.28}, t), 0.7);
    }

    Vec hit = vadd(orig, vscale(dir, best_t));
    Vec nrm, base;
    double reflect;

    if (floor_hit) {
        nrm = (Vec){0,1,0};
        int cw = ((int)floor(hit.x) + (int)floor(hit.z)) & 1;
        base = cw ? CHECK_A : CHECK_B;
        double d = vlen(hit);
        double fade = 1.0 / (1.0 + d*d*0.02);
        base = vlerp((Vec){0.45,0.48,0.55}, base, fade);
        reflect = 0.15 * fade;              /* el suelo refleja un poco */
    } else {
        Sphere *s = &sc->spheres[hit_idx];
        nrm = vnorm(vsub(hit, s->center));
        base = s->color;
        reflect = s->reflect;
        if (s->emissive > 0)
            return vadd(base, vscale((Vec){1,1,1}, s->emissive));
    }

    /* iluminación directa */
    Vec col = {0,0,0};
    for (int li = 0; li < sc->nlights; li++) {
        Light *L = &sc->lights[li];
        Vec toL = vsub(L->pos, hit);
        double distL = vlen(toL);
        Vec ldir = vscale(toL, 1.0/distL);
        double ndl = vdot(nrm, ldir);
        if (ndl <= 0) continue;

        int shadow = 0;
        Vec sho = vadd(hit, vscale(nrm, 1e-4));
        for (int i = 0; i < sc->nspheres && !shadow; i++) {
            if (i == hit_idx) continue;
            double t = intersect_sphere(&sc->spheres[i], sho, ldir);
            if (t > 0 && t < distL) shadow = 1;
        }
        if (shadow) continue;

        double att = L->intensity / (1.0 + distL*distL*0.002);
        /* difusa */
        col = vadd(col, vmul(vscale(L->color, ndl*att), base));
        /* especular Blinn-Phong */
        Vec h = vnorm(vsub(ldir, dir));
        double spec = pow(fmax(0, vdot(nrm, h)), 64.0);
        col = vadd(col, vscale(L->color, spec*att*0.4));
    }

    /* luz ambiental + cielo difuso */
    double sky = 0.5 + 0.5*nrm.y;
    col = vmul(vadd(col, vscale((Vec){0.35,0.42,0.55}, sky*0.22)), base);

    /* reflexión recursiva */
    if (reflect > 0.01 && depth < 5) {
        Vec rdir = vsub(dir, vscale(nrm, 2.0*vdot(dir, nrm)));
        Vec rcol = trace(sc, vadd(hit, vscale(nrm, 1e-4)), rdir, depth+1);
        col = vadd(vscale(col, 1.0-reflect), vscale(rcol, reflect));
    }
    return col;
}

typedef struct { const Scene *sc; int start, end; } Job;

static void *worker(void *arg) {
    Job *j = (Job*)arg;
    const Scene *sc = j->sc;
    Vec fwd = vnorm(vsub(sc->cam_look, sc->cam_pos));
    Vec right = vnorm((Vec){ fwd.z, 0.0, -fwd.x });
    Vec cr = (Vec){ right.y*fwd.z - right.z*fwd.y,
                    right.z*fwd.x - right.x*fwd.z,
                    right.x*fwd.y - right.y*fwd.x };
    Vec up = vnorm((Vec){-cr.x, -cr.y, -cr.z});
    double aspect = (double)sc->width / sc->height;

    for (int py = j->start; py < j->end; py++) {
        for (int px = 0; px < sc->width; px++) {
            Vec acc = {0,0,0};
            for (int sy = 0; sy < sc->samples; sy++) {
                for (int sx = 0; sx < sc->samples; sx++) {
                    double u = ((px + (sx+0.5)/sc->samples) / sc->width  - 0.5) * 2.0 * sc->fov * aspect;
                    double v = (0.5 - (py + (sy+0.5)/sc->samples) / sc->height) * 2.0 * sc->fov;
                    Vec d = vnorm(vadd(fwd, vadd(vscale(right,u), vscale(up,v))));
                    acc = vadd(acc, trace(sc, sc->cam_pos, d, 0));
                }
            }
            acc = vscale(acc, 1.0/(sc->samples*sc->samples));
            /* gamma + exposición */
            double g = 1.0/2.2;
            uint8_t *px8 = &sc->pixels[(py*sc->width + px)*3];
            px8[0] = (uint8_t)(255.0 * pow(clamp01(acc.x), g));
            px8[1] = (uint8_t)(255.0 * pow(clamp01(acc.y), g));
            px8[2] = (uint8_t)(255.0 * pow(clamp01(acc.z), g));
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    enum { W = 960, H = 540, SAMPLES = 2, NTH = 8 };
    double theta = (argc > 1) ? atof(argv[1]) * M_PI / 180.0 : 0.0;
    double cx = 0.0, cy = 1.9, cz = 5.9;   /* centro de la escena */
    double R = 5.0;                         /* radio orbital */

    Sphere spheres[] = {
        /* cromada grande */
        {{ -1.9, 1.0,  6.0 }, {0.90,0.92,0.95}, 1.0, 0.92, 0 },
        /* roja difusa */
        {{  0.6, 0.7,  7.2 }, {0.95,0.25,0.20}, 0.7, 0.10, 0 },
        /* ámbar emisiva */
        {{  2.4, 0.45, 5.6 }, {1.00,0.65,0.10}, 0.45, 0.0, 2.2 },
        /* verde espejo pequeña flotando */
        {{  1.4, 2.1,  4.2 }, {0.25,0.90,0.45}, 0.5, 0.65, 0 },
    };
    Light lights[] = {
        {{ -6.0, 8.0, 4.0 }, {1.0,0.98,0.92}, 1.4 },
        {{  7.0, 6.0, 1.0 }, {0.35,0.45,0.85}, 0.8 },
    };

    Scene sc = {
        .spheres = spheres, .nspheres = 4,
        .lights = lights, .nlights = 2,
        .width = W, .height = H, .samples = SAMPLES,
        .cam_pos = {cx + R*sin(theta), cy, cz + R*cos(theta)},
        .cam_look = {0.0, 0.9, 5.8},
        .fov = 0.8,
    };
    sc.pixels = malloc((size_t)W*H*3);

    pthread_t th[NTH]; Job jobs[NTH];
    int chunk = H / NTH;
    for (int i = 0; i < NTH; i++) {
        jobs[i] = (Job){ &sc, i*chunk, (i == NTH-1) ? H : (i+1)*chunk };
        pthread_create(&th[i], NULL, worker, &jobs[i]);
    }
    for (int i = 0; i < NTH; i++) pthread_join(th[i], NULL);

    printf("P6\n%d %d\n255\n", W, H);
    fwrite(sc.pixels, 1, (size_t)W*H*3, stdout);
    return 0;
}
