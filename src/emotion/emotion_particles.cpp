#include "emotion_engine.h"
#include "config.h"
#include "clock/clock_engine.h"
#include "bitmaps/icons.h"
#include <math.h>

void EmotionEngine::updateParticles(uint32_t deltaMs) {
    float dt = deltaMs / 1000.0f;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!_particles[i].active) continue;

        _particles[i].x += _particles[i].vx * dt;
        _particles[i].y += _particles[i].vy * dt;
        _particles[i].life -= dt;

        // Deactivate if expired or off screen
        if (_particles[i].life <= 0 ||
            _particles[i].y < -10 || _particles[i].y > 70 ||
            _particles[i].x < -10 || _particles[i].x > 138) {
            _particles[i].active = false;
        }
    }
}

void EmotionEngine::spawnParticle(uint8_t type) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (_particles[i].active) continue;

        _particles[i].active = true;
        _particles[i].type = type;
        _particles[i].life = 2.0f + random(100) / 100.0f;

        if (type == 0) {
            // Hearts: float up from sides
            _particles[i].x = random(20, 108);
            _particles[i].y = 50 + random(10);
            _particles[i].vx = random(-10, 10);
            _particles[i].vy = -15 - random(10);
        } else if (type == 1) {
            // Stars: sparkle around the head area
            _particles[i].x = random(25, 103);
            _particles[i].y = 10 + random(20);
            _particles[i].vx = random(-8, 8);
            _particles[i].vy = -10 - random(5);
        } else if (type == 2) {
            // Zzz: drift up-right from head area
            _particles[i].x = 85 + random(10);
            _particles[i].y = 15 + random(5);
            _particles[i].vx = 5 + random(5);
            _particles[i].vy = -8 - random(5);
        }
        break;
    }
}

// =============================================================
// Rendering
// =============================================================

