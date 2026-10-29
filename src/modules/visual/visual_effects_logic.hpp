#pragma once
#include <cmath>
#include <cstddef>
namespace visual_effects_logic
{
    inline constexpr std::size_t max_particles = 96, max_circles = 8, max_shape_boxes = 24;
    class jump_detector
    {
        bool initialized_ = false, grounded_ = false;
    public:
        int sample(bool grounded, bool rising)
        {
            int event = 0;
            if (initialized_) {
                if (grounded_ && !grounded && rising) event = 1;
                else if (!grounded_ && grounded) event = 2;
            }
            initialized_ = true; grounded_ = grounded; return event;
        }
        void reset() { initialized_ = grounded_ = false; }
    };
    class hit_detector
    {
        int ticks_ = -1, id_ = -1, hurt_ = 0;
        float health_ = 0.f;
        double armed_until_ = -1, damage_at_ = -1, time_ = -1;
    public:
        bool pending(double now) const { return now >= 0 && now <= armed_until_; }
        bool observe_attack(int attack_ticks, double now)
        {
            if (!std::isfinite(now) || now < 0 || now < time_ || attack_ticks < 0) {
                reset(); return false;
            }
            const bool attack = ticks_ >= 0 && attack_ticks < ticks_;
            ticks_ = attack_ticks; time_ = now;
            if (attack && id_ >= 0) armed_until_ = now + .5;
            return attack;
        }
        bool sample(int attack_ticks, int id, int hurt, float health, double now)
        {
            if (!std::isfinite(now) || now < 0 || now < time_ || !std::isfinite(health) || health < 0 || hurt < 0 || attack_ticks < 0) {
                reset(); return false;
            }
            observe_attack(attack_ticks, now);
            if (id != id_ || id < 0) {
                id_ = id; hurt_ = hurt; health_ = health; armed_until_ = damage_at_ = -1;
                return false; // Acquiring an already damaged target is not our hit.
            }
            const bool damage = hurt > hurt_ || health < health_ - .01f;
            // Damage and the local cooldown reset can reach the renderer in
            // opposite order. Keep a short observation, not an unbounded hit.
            if (damage) damage_at_ = now;
            const bool hit = pending(now) && damage_at_ >= 0 && now - damage_at_ <= .075;
            hurt_ = hurt; health_ = health;
            if (hit) armed_until_ = damage_at_ = -1;
            return hit;
        }
        void reset() { ticks_ = id_ = -1; hurt_ = 0; health_ = 0; armed_until_ = damage_at_ = time_ = -1; }
    };
}
