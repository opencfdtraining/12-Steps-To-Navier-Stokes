#include <vector>
#include <SDL.h>
#include "Core.h"
#include "Vector2d.h"
#include "GraphMetrics.h"
#include "FiniteDifference.h"
#include "Render.h"
#include "Step11.h"

Step11CavityFlow::Step11CavityFlow(const int windowWidth, const int windowHeight)
{
    title = "Step 11: Cavity Flow";

    fixedTimeStep = 1.0 / 60.0;

    graphMetrics.width = windowWidth - 20;
    graphMetrics.height = windowHeight - 20;
    graphMetrics.pos = Vector2d(10, 10);
    graphMetrics.minX = 0;
    graphMetrics.maxX = 2;
    graphMetrics.minY = 0;
    graphMetrics.maxY = 2;

    dx = (graphMetrics.maxX - graphMetrics.minX) / (numX - 1);
    dy = (graphMetrics.maxY - graphMetrics.minY) / (numY - 1);

    p = zeros(numX, numY);
    v = std::vector<std::vector<Vector2d>>(numX, std::vector<Vector2d>(numY, Vector2d(0, 0)));

    heightMap = NULL;
}

void Step11CavityFlow::applyPBoundaryConditions()
{
    // dp/dy = 0 at y = 0
    for(size_t i = 0; i < numX; i++)
    {
        p[i][0] = p[i][1];
    }

    // p = 0 at y = 2
    for(size_t i = 0; i < numX; i++)
    {
        p[i][numY - 1] = 0;
    }

    // dp/dx = 0 at x = 0, 2
    for(size_t i = 0; i < numX; i++)
    {
        // dp/dx = 0 at x = 0
        p[0][i] = p[1][i];

        // dp/dx = 0 at x = 2
        p[numX - 1][i] = p[numX - 2][i];
    }
}
void Step11CavityFlow::updateP(const double dt)
{
    for(size_t iter = 0; iter < numPIterations; iter++)
    {
        const auto b = getBForIncompressibleNavierStokes(v, rho, numX, numY, dx, dy, dt);
        p = iteratePoissonsEquation(p, b, numX, numY, dx, dy);
        applyPBoundaryConditions();
    }
}

void Step11CavityFlow::applyFlowVelocityBoundaryConditions()
{
    // v = 0 at boundaries
    for(size_t i = 0; i < numX; i++)
    {
        v[0][i] = Vector2d(0, 0); // v = 0 at x = 0
        v[numX - 1][i] = Vector2d(0, 0); // v = 0 at x = 2
        v[i][0] = Vector2d(0, 0); // v = 0 at y = 0
        v[i][numY - 1] = Vector2d(0, 0); // v = 0 at y = 2
    }

    // vx = 1 at y = 2
    for(size_t i = 0; i < numX; i++)
    {
        v[i][numY - 1].x = 1;
    }
}
void Step11CavityFlow::updateFlowVelocity(const double dt)
{
    auto newV = v;
    for(size_t i = 1; i < numX - 1; i++)
    {
        for(size_t j = 1; j < numY - 1; j++)
        {
            const auto dvdx = (v[i][j] - v[i - 1][j]) / dx;
            const auto dvdy = (v[i][j] - v[i][j - 1]) / dy;
            const auto convectiveTerm = (v[i][j].x * dvdx) + (v[i][j].y * dvdy);
            const auto gradientOfP = gradient1stOrderCentralDiff(p, i, j, dx, dy);
            const auto laplacianOfV = laplacian2ndOrderCentralDiff(v, i, j, dx, dy);

            const auto dvdt = -convectiveTerm
                - ((1.0 / rho) * gradientOfP)
                + (nu * laplacianOfV);

            newV[i][j] = v[i][j] + (dt * (dvdt));
        }
    }

    v = newV;

    applyFlowVelocityBoundaryConditions();
}

void Step11CavityFlow::update(const double dt)
{
    const auto scaledDt = timeScale * dt;

    updateP(scaledDt);
    updateFlowVelocity(scaledDt);
}

void Step11CavityFlow::draw(SDL_Renderer* renderer)
{
    if(heightMap == NULL)
    {
        heightMap = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, numX, numY);
    }

    updateHeightmap(heightMap, numX, numY, p, minP, maxP);

    SDL_Rect graphRect;
    graphRect.w = (int)graphMetrics.width;
    graphRect.h = (int)graphMetrics.height;
    graphRect.x = (int)graphMetrics.pos.x;
    graphRect.y = (int)graphMetrics.pos.y;

    SDL_RenderCopy(renderer, heightMap, NULL, &graphRect);

    const auto pixelHeight = (graphMetrics.maxY - graphMetrics.minY) / numY;
    const auto maxVNorm = (3.0 / 4.0) * pixelHeight;
    renderVectorField(renderer, v, graphMetrics, numX, numY, 1, maxVNorm);
}