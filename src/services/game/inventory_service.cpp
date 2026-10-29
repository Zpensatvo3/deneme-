#include "inventory_service.hpp"

#include "../../platform/jni/jni_context.hpp"

#include <algorithm>
#include <string>

namespace
{
    bool translation_matches(const std::string& key, services::inventory::hotbar_item_kind kind)
    {
        switch (kind)
        {
        case services::inventory::hotbar_item_kind::sword:
            return key.ends_with("_sword") || key.ends_with(".sword");
        case services::inventory::hotbar_item_kind::axe:
            return key.ends_with("_axe") || key.ends_with(".axe");
        }
        return false;
    }
}

bool services::inventory::slot_matches(const maps::DefaultedList& items,
    int slot, hotbar_item_kind kind)
{
    if (!items || slot < 0 || slot > 8)
        return false;

    maps::Object object = items.get.call(static_cast<jint>(slot));
    if (!object || platform::jni_context::clear_exception())
        return false;
    maps::ItemStack stack(object.object_instance, false);
    if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
        return false;
    maps::Item item = stack.item.get();
    if (!item || platform::jni_context::clear_exception())
        return false;

    bool class_match = false;
    switch (kind)
    {
    case hotbar_item_kind::sword: class_match = item.is_sword(); break;
    case hotbar_item_kind::axe: class_match = item.is_instance_of<maps::AxeItem>(); break;
    }
    platform::jni_context::clear_exception();

    maps::String translation = item.getTranslationKey.call();
    const std::string key = translation && !platform::jni_context::clear_exception()
        ? translation.to_string()
        : std::string{};
    platform::jni_context::clear_exception();
    return class_match || translation_matches(key, kind);
}

int services::inventory::find_hotbar_slot(const maps::DefaultedList& items,
    hotbar_item_kind kind, int preferred_slot)
{
    if (preferred_slot >= 0)
    {
        preferred_slot = std::clamp(preferred_slot, 0, 8);
        if (slot_matches(items, preferred_slot, kind))
            return preferred_slot;
    }

    for (int slot = 0; slot < 9; ++slot)
        if (slot != preferred_slot && slot_matches(items, slot, kind))
            return slot;
    return -1;
}
