#pragma once
#include <string>
#include <string_view>

namespace platform::keys
{
    // Existing config values are Windows VKs. One explicit extension separates
    // keypad Enter (E0 1C) from main Enter without replacing the binding system.
    inline constexpr int num_enter = 0x10d;
    struct key { int code; const char* label; float x, y, w = 1, h = 1; unsigned scan = 0; };
    inline constexpr key keyboard[] = {
        {0x1b,"Esc",0,0}, {0x70,"F1",2,0}, {0x71,"F2",3,0}, {0x72,"F3",4,0}, {0x73,"F4",5,0},
        {0x74,"F5",6.5f,0}, {0x75,"F6",7.5f,0}, {0x76,"F7",8.5f,0}, {0x77,"F8",9.5f,0},
        {0x78,"F9",11,0}, {0x79,"F10",12,0}, {0x7a,"F11",13,0}, {0x7b,"F12",14,0},
        {0x2c,"PrtSc",15.5f,0}, {0x91,"ScrLk",16.5f,0}, {0x13,"Pause",17.5f,0},
        {0xc0,"`",0,1.5f}, {'1',"1",1,1.5f}, {'2',"2",2,1.5f}, {'3',"3",3,1.5f},
        {'4',"4",4,1.5f}, {'5',"5",5,1.5f}, {'6',"6",6,1.5f}, {'7',"7",7,1.5f},
        {'8',"8",8,1.5f}, {'9',"9",9,1.5f}, {'0',"0",10,1.5f}, {0xbd,"-",11,1.5f},
        {0xbb,"=",12,1.5f}, {0x08,"Backspace",13,1.5f,2},
        {0x2d,"Ins",15.5f,1.5f,1,1,0x152}, {0x24,"Home",16.5f,1.5f,1,1,0x147}, {0x21,"PgUp",17.5f,1.5f,1,1,0x149},
        {0x90,"Num",19,1.5f}, {0x6f,"/",20,1.5f,1,1,0x135}, {0x6a,"*",21,1.5f,1,1,0x37}, {0x6d,"-",22,1.5f,1,1,0x4a},
        {0x09,"Tab",0,2.5f,1.5f}, {'Q',"Q",1.5f,2.5f}, {'W',"W",2.5f,2.5f}, {'E',"E",3.5f,2.5f},
        {'R',"R",4.5f,2.5f}, {'T',"T",5.5f,2.5f}, {'Y',"Y",6.5f,2.5f}, {'U',"U",7.5f,2.5f},
        {'I',"I",8.5f,2.5f}, {'O',"O",9.5f,2.5f}, {'P',"P",10.5f,2.5f},
        {0xdb,"[",11.5f,2.5f}, {0xdd,"]",12.5f,2.5f}, {0xdc,"\\",13.5f,2.5f,1.5f},
        {0x2e,"Del",15.5f,2.5f,1,1,0x153}, {0x23,"End",16.5f,2.5f,1,1,0x14f}, {0x22,"PgDn",17.5f,2.5f,1,1,0x151},
        {0x67,"7",19,2.5f,1,1,0x47}, {0x68,"8",20,2.5f,1,1,0x48}, {0x69,"9",21,2.5f,1,1,0x49}, {0x6b,"+",22,2.5f,1,2,0x4e},
        {0x14,"Caps",0,3.5f,1.75f}, {'A',"A",1.75f,3.5f}, {'S',"S",2.75f,3.5f}, {'D',"D",3.75f,3.5f},
        {'F',"F",4.75f,3.5f}, {'G',"G",5.75f,3.5f}, {'H',"H",6.75f,3.5f}, {'J',"J",7.75f,3.5f},
        {'K',"K",8.75f,3.5f}, {'L',"L",9.75f,3.5f}, {0xba,";",10.75f,3.5f}, {0xde,"'",11.75f,3.5f},
        {0x0d,"Enter",12.75f,3.5f,2.25f,1,0x1c},
        {0x64,"4",19,3.5f,1,1,0x4b}, {0x65,"5",20,3.5f,1,1,0x4c}, {0x66,"6",21,3.5f,1,1,0x4d},
        {0xa0,"Shift",0,4.5f,2.25f}, {'Z',"Z",2.25f,4.5f}, {'X',"X",3.25f,4.5f}, {'C',"C",4.25f,4.5f},
        {'V',"V",5.25f,4.5f}, {'B',"B",6.25f,4.5f}, {'N',"N",7.25f,4.5f}, {'M',"M",8.25f,4.5f},
        {0xbc,",",9.25f,4.5f}, {0xbe,".",10.25f,4.5f}, {0xbf,"/",11.25f,4.5f}, {0xa1,"Shift",12.25f,4.5f,2.75f},
        {0x26,"Up",16.5f,4.5f,1,1,0x148},
        {0x61,"1",19,4.5f,1,1,0x4f}, {0x62,"2",20,4.5f,1,1,0x50}, {0x63,"3",21,4.5f,1,1,0x51}, {num_enter,"Enter",22,4.5f,1,2,0x11c},
        {0xa2,"Ctrl",0,5.5f,1.25f}, {0x5b,"Win",1.25f,5.5f,1.25f}, {0xa4,"Alt",2.5f,5.5f,1.25f},
        {0x20,"Space",3.75f,5.5f,6.25f}, {0xa5,"Alt",10,5.5f,1.25f}, {0x5c,"Win",11.25f,5.5f,1.25f},
        {0x5d,"Menu",12.5f,5.5f,1.25f}, {0xa3,"Ctrl",13.75f,5.5f,1.25f},
        {0x25,"Left",15.5f,5.5f,1,1,0x14b}, {0x28,"Down",16.5f,5.5f,1,1,0x150}, {0x27,"Right",17.5f,5.5f,1,1,0x14d},
        {0x60,"0",19,5.5f,2,1,0x52}, {0x6e,".",21,5.5f,1,1,0x53}
    };
    static_assert(sizeof(keyboard) / sizeof(keyboard[0]) == 104);
    constexpr bool valid(int code) { return (code >= 0 && code <= 255) || code == num_enter; }
    constexpr const key* find(int code) {
        for (const auto& item : keyboard) if (item.code == code) return &item;
        return nullptr;
    }
    constexpr unsigned scan_for(int code) { const auto* item = find(code); return item ? item->scan : 0; }
    constexpr const char* reserved_reason(int code) {
        if (code == 0x1b) return "Esc clears a binding or closes the menu.";
        if (code == 0xa1) return "Right Shift is the fixed menu shortcut.";
        if (code == 0x23) return "End is the emergency unload shortcut.";
        if (code == 0x5b || code == 0x5c) return "Windows keys are reserved for the operating system.";
        return nullptr;
    }
    constexpr bool assignable(int code) { return find(code) && !reserved_reason(code); }
    inline std::string label(int code) {
        if (code == 0) return "none";
        if (code == num_enter) return "Num Enter";
        if (code >= 0x60 && code <= 0x69) return "Num " + std::to_string(code - 0x60);
        if (code == 0x6a) return "Num *";
        if (code == 0x6b) return "Num +";
        if (code == 0x6d) return "Num -";
        if (code == 0x6e) return "Num .";
        if (code == 0x6f) return "Num /";
        if (code == 0xa0) return "Left Shift";
        if (code == 0xa1) return "Right Shift";
        if (code == 0xa2) return "Left Ctrl";
        if (code == 0xa3) return "Right Ctrl";
        if (code == 0xa4) return "Left Alt";
        if (code == 0xa5) return "Right Alt";
        if (const auto* item = find(code)) return item->label;
        if (code >= 1 && code <= 6) return "Mouse " + std::to_string(code);
        return "VK " + std::to_string(code);
    }
    constexpr int from_message(int vk, unsigned scan) {
        if (vk == 0x10) return (scan & 0xff) == 0x36 ? 0xa1 : 0xa0;
        if (vk == 0x11) return scan & 0x100 ? 0xa3 : 0xa2;
        if (vk == 0x12) return scan & 0x100 ? 0xa5 : 0xa4;
        if (vk == 0x0d) return scan & 0x100 ? num_enter : 0x0d;
        // NumLock-off keypad messages use navigation VKs but keep keypad scans.
        for (const auto& item : keyboard)
            if (item.code >= 0x60 && item.code <= 0x6f && item.scan && item.scan == scan) return item.code;
        return vk;
    }
}
