#include <tsgl.h>
#include "boids.hpp"
#include "misc.h"
#include <omp.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <memory>

using namespace tsgl;

class boid {
private:
    std::unique_ptr<Arrow> _arrow;
    Canvas& _can;
public:
    /**
     * @brief Construct a new boid object, which is only ever stored on the CPU.
     * We use pointer arrays so we don't have to send my boid objects to the GPU.
     * 
     * @param x 
     * @param y 
     * @param can 
     */
    boid(float x, float y, int index, Canvas& can) : _can(can) {
        _arrow = std::make_unique<Arrow>(
            x, y, 0,
            20, 20,
            0, 0, 0,
            CYAN
        );
        _can.add(_arrow.get());
    }

    /**
     * @brief Destroy the boid object
     * 
     */
    ~boid() {
        _can.remove(_arrow.get());
    }

    void setColor(tsgl::ColorFloat color) {
        color.A = 0.9;
        _arrow->setColor(color);
    }

    void updatePosition(float x, float y) {
        _arrow->setCenter(x, y, 0);
    }

    void updateDirection(float velx, float vely) {
        float yaw = atan(vely / velx) * 180. / PI;
        if (velx > 0) {
            yaw += 180;
        }
        _arrow->setYaw(yaw);
    }
};

struct boids::Params p;

struct boids::Params defaultParams = 
    {
		.width = 1024, .height = 1024, 
		.num = 1024, .len = 20, 
		.mag = 1, .seed = 0, 
		.invert = 0, .steps = 100000000, 
		.psdump = 0, .angle = 270.0, 
		.vangle = 90, .minv = 0.5, 
		.ddt = 0.95, .dt = 3.0, 
		.rcopy = 80, .rcent = 30, 
		.rviso = 40, .rvoid = 15, 
		.wcopy = 0.2, .wcent = 0.4, 
		.wviso = 0.8, .wvoid = 1.0, .
		threads = 1, .term = NULL
    };

float* xp;
float* yp;
float* xv;
float* yv;
float* xnv;
float* ynv;

ColorFloat arr[] = {WHITE, BLUE, CYAN, YELLOW, GREEN, ORANGE, LIME, PURPLE};

void initiateBoidArrays(
    struct boids::Params p, 
    float* xp, float* yp,
    float* xv, float* yv
) 
{
    for (int i = 0; i < p.num; ++i) {
        xp[i] = random_range(-p.width/2, p.width/2);
        yp[i] = random_range(-p.height/2, p.height/2);
        xv[i] = random_range(-1., 1.);
        yv[i] = random_range(-1., 1.);
        boids::norm(&xv[i], &yv[i]);
    }
}

void initiateBoidDraw(
    struct boids::Params p,
    std::vector<std::unique_ptr<boid>>& boidDraw,
    float* xp, float* yp,
    float* xv, float* yv,
    Canvas& canvasP
)
{
    for (int i = 0; i < p.num; ++i) {
        boidDraw[i] = std::make_unique<boid>(xp[i], yp[i], i, canvasP);
        boidDraw[i]->updateDirection(xv[i], yv[i]);
    }
}

void boidIteration (
    boids::Params p,
    float* xp, float* yp,
    float* xv, float* yv,
    float* xnv, float* ynv
) 
{
    boids::compute_new_headings(p, xp, yp, xv, yv, xnv, ynv);

    for (int i = 0; i < p.num; ++i) 
    {
        xv[i] = xnv[i];
        yv[i] = ynv[i];
        xp[i] += xv[i] * p.dt;
        yp[i] += yv[i] * p.dt;
        
        
        // Wrap around screen coordinates
        if (xp[i] < -p.width / 2) {
            xp[i] += p.width;
        }
        else if (xp[i] >= p.width / 2) {
            xp[i] -= p.width;
        }

        if (yp[i] < -p.height / 2) {
            yp[i] += p.height;
        }
        else if (yp[i] >= p.height / 2) {
            yp[i] -= p.height;
        }
    }
}

void boidDrawIteration (
    boids::Params p,
    float* xp, float* yp,
    float* xv, float* yv,
    float* xnv, float* ynv,
    std::vector<std::unique_ptr<boid>>& boidDraw
)
{
    boids::compute_new_headings(p, xp, yp, xv, yv, xnv, ynv);

    #ifndef GPU
    #pragma acc parallel loop independent collapse(1) num_gangs(p.threads)
    #endif
    for (int i = 0; i < p.num; ++i) {
        xv[i] = xnv[i];
        yv[i] = ynv[i];
        xp[i] += xv[i] * p.dt;
        yp[i] += yv[i] * p.dt;
        
        
        // Wrap around screen coordinates
        if (xp[i] < -p.width / 2) {
            xp[i] += p.width;
        }
        else if (xp[i] >= p.width / 2) {
            xp[i] -= p.width;
        }

        if (yp[i] < -p.height / 2) {
            yp[i] += p.height;
        }
        else if (yp[i] >= p.height / 2) {
            yp[i] -= p.height;
        }


        boidDraw[i]->updatePosition(xp[i], yp[i]);
        boidDraw[i]->updateDirection(xv[i], yv[i]);

        boidDraw[i]->setColor(arr[omp_get_thread_num() % 8]);
    }
}

void tsglScreen(Canvas& canvas) {
    initiateBoidArrays(p, xp, yp, xv, yv);

    std::vector<std::unique_ptr<boid>> boidDraw(p.num);
    initiateBoidDraw(p, boidDraw, xp, yp, xv, yv, canvas);

    while (canvas.isOpen()) {
        // canvas.sleep();


        boidDrawIteration(p, xp, yp, xv, yv, xnv, ynv, boidDraw);
    }
}

int main(int argc, char* argv[]) {
    p = defaultParams;

    // OPTION* o = boids::setOptions(p);
    // get_options(argc, argv, o, "test");

    p.num = 128;

    p.width = 1920;
    p.height = 1080;

    if (argc > 1) {
        p.threads = atoi(argv[1]);
    }

    if (argc > 2) {
        p.num = atoi(argv[2]);
    }

    xp  = new float[p.num];
    yp  = new float[p.num];
    xv  = new float[p.num];
    yv  = new float[p.num];
    xnv = new float[p.num];
    ynv = new float[p.num];
    

    Canvas can(-1, -1, p.width, p.height, "Test Screen", BLACK);
    can.run(tsglScreen);

    // Testing
    // initiateBoidArrays(p, xp, yp, xv, yv);
    // fprintf(stderr, "Boid size of %d starting\n", p.num);
    // double t1 = omp_get_wtime();
    // for (int i = 0; i < 1000; ++i) {
    //     boidIteration(p, xp, yp, xv, yv, xnv, ynv);
    //     if (i % 50 == 0) {
    //         fprintf(stderr, "\tit %d done\n", i);
    //     }
    // }
    // double t2 = omp_get_wtime();
    // fprintf(stdout, "%lf", t2 - t1);
    // fprintf(stderr, "\n%lf\n\n", t2 - t1);

    


    delete [] xp;
    delete [] yp;
    delete [] xv;
    delete [] yv;
    delete [] xnv;
    delete [] ynv;
}
