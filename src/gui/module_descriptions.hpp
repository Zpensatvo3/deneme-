#pragma once

#include <cstring>

namespace gui
{
    inline const char* module_description(const char* name)
    {
        if (!name) return "";
        struct entry { const char* name; const char* english; };
        static constexpr entry descriptions[]{
            { "friends manager", "Manage your friends list" },
            { "click gui", "Menu controls and shortcuts" },
            { "self destruct", "Close the client" },
            { "anti bot", "Skip suspected fake players" },
            { "click aim assist", "Track targets strongly while clicking" },
            { "aim assist", "Help you aim at a target" },
            { "triggerbot", "Attack the target you aim at" },
            { "jump reset", "Jump after taking a hit" },
            { "shield breaker", "Switch to an axe against shields" },
            { "auto totem", "Put a totem in your hand" },
            { "pearl macro", "Throw a pearl and switch back" },
            { "sprint", "Sprint automatically while moving" },
            { "no jump delay", "Jump again without waiting" },
            { "fast place", "Place blocks and use potions faster" },
            { "fast exp", "Throw experience bottles faster" },
            { "pot refill", "Move healing potions to empty hotbar slots" },
            { "mouse tweaks", "Move items easily by dragging" },
            { "watermark", "Show the client logo" },
            { "arraylist", "List your enabled modules" },
            { "chams", "Color player models" },
            { "china hat", "Wear a cone hat in third person" },
            { "wings", "Wear animated wings in third person" },
            { "cape", "Wear a cape that sways as you move" },
            { "sky", "Change the sky and horizon colors" },
            { "trails", "Leave particles and footprints as you move" },
            { "hit particles", "Show particles when your target takes a hit" },
            { "jump circles", "Show rings when you jump or land" },
            { "player esp", "Mark players with boxes and labels" },
            { "item esp", "Mark items on the ground" },
            { "storage esp", "Mark chests and other storage blocks" },
            { "block esp", "Highlight selected blocks with 3D boxes" },
            { "radar", "Show nearby players on a radar" },
            { "full bright", "Make dark areas brighter" },
            { "spotify hud", "Show your Spotify song and lyrics" },
            { "vape aim assist", "Help you aim at a target" },
        };
        for (const auto& description : descriptions)
            if (std::strcmp(description.name, name) == 0)
                return description.english;
        return "Client module";
    }
}
