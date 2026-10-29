#pragma once
// Shared SDK layout; target-specific JNI differences are selected at compile time.
#include "world_blocks.hpp"

// Shared Fabric bindings with compile-time target profiles.
namespace maps
{
        BEGIN_KLASS_DEF(Packet, ONESTEP_MC_NAME("net/minecraft/class_2596"))
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(GameProfile, "com/mojang/authlib/GameProfile")
        jni::method<String, bindings_profile::profile_name, jni::NOT_STATIC> getName{ *this };
        jni::method<UUID, bindings_profile::profile_id> getId{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerListEntry, ONESTEP_MC_NAME("net/minecraft/class_640"))
        jni::method<GameProfile, ONESTEP_MC_NAME("method_2966")> getProfile{ *this };
        jni::method<jint, ONESTEP_MC_NAME("method_2959")> getLatency{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ClientPlayNetworkHandler, ONESTEP_MC_NAME("net/minecraft/class_634"))
        jni::method<void, ONESTEP_MC_NAME("method_2883"), jni::NOT_STATIC, Packet> sendPacket{ *this };
        jni::method<Collection, ONESTEP_MC_NAME("method_2880")> getPlayerList{ *this };
        jni::method<PlayerListEntry, ONESTEP_MC_NAME("method_2874"), jni::NOT_STATIC, String> getPlayerListEntryByName{ *this };
        jni::method<PlayerListEntry, ONESTEP_MC_NAME("method_2871"), jni::NOT_STATIC, UUID> getPlayerListEntry{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerActionC2SPacket$Action, ONESTEP_MC_NAME("net/minecraft/class_2846$class_2847"))
        jni::field<PlayerActionC2SPacket$Action, ONESTEP_MC_NAME("field_12969"), jni::STATIC> SWAP_ITEM_WITH_OFFHAND{ *this };
    jni::field<PlayerActionC2SPacket$Action, ONESTEP_MC_NAME("field_12974"), jni::STATIC> RELEASE_USE_ITEM{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(PlayerActionC2SPacket, ONESTEP_MC_NAME("net/minecraft/class_2846"))
        jni::constructor<PlayerActionC2SPacket$Action, BlockPos, Direction> constructor{ *this };
    END_KLASS_DEF()

	BEGIN_KLASS_DEF(ComponentType, ONESTEP_MC_NAME("net/minecraft/class_9331"))
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(ChargedProjectilesComponent, ONESTEP_MC_NAME("net/minecraft/class_9278"))
		jni::method<jboolean, ONESTEP_MC_NAME("method_57442")> isEmpty{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_57438"), jni::NOT_STATIC, Item> contains{ *this };
	END_KLASS_DEF()

	BEGIN_KLASS_DEF(RegistryEntry, ONESTEP_MC_NAME("net/minecraft/class_6880"))
	END_KLASS_DEF()

        BEGIN_KLASS_DEF(ItemStack, ONESTEP_MC_NAME("net/minecraft/class_1799"))
#if ONESTEP_MINECRAFT_26_1
        // The backing field is now Holder<Item>. Keep native callers' item.get()
        // contract, using vanilla's public getter instead of guessing a holder.
        jni::method<Item, "getItem"> getItem{*this};
        struct item_access
        {
            const ItemStack_members& owner;
            bool is_valid() const { return owner.getItem.is_valid(); }
            Item get() const { return owner.getItem.call(); }
        } item{*this};
#else
        jni::field<Item, ONESTEP_MC_NAME("field_8038")> item{ *this };
#endif
        jni::method<jboolean, ONESTEP_MC_NAME("method_7960")> isEmpty{ *this };
        jni::method<ItemStack, ONESTEP_MC_NAME("method_7971"), jni::NOT_STATIC, jint> split{ *this };
#if ONESTEP_MINECRAFT_26_1
        // TypedInstance<T>.is(T) erases T to Object in the JVM descriptor.
        jni::method<jboolean, ONESTEP_MC_NAME("method_31574"), jni::NOT_STATIC, Object> matchesItem{*this};
        struct item_comparison
        {
            const ItemStack_members& owner;
            bool is_valid() const { return owner.matchesItem.is_valid(); }
            jboolean call(const Item& value) const { return owner.matchesItem.call(Object(value.object_instance, false)); }
        } isOf{*this};
#else
        jni::method<jboolean, ONESTEP_MC_NAME("method_31574"), jni::NOT_STATIC, Item> isOf{ *this };
#endif
        jni::method<ItemStack, ONESTEP_MC_NAME("method_7972"), jni::NOT_STATIC> copy{ *this };
        jni::method<jint, ONESTEP_MC_NAME("method_7947")> getCount{ *this };
        jni::method<Text, ONESTEP_MC_NAME("method_7964")> getName{ *this };
        jni::method<jint, ONESTEP_MC_NAME("method_7914")> getMaxCount{ *this };
        // ItemStack: remaining durability and enchantment indicator.
        jni::method<jint, ONESTEP_MC_NAME("method_7919")> getDamage{ *this };
        jni::method<jint, ONESTEP_MC_NAME("method_7936")> getMaxDamage{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_7942")> hasEnchantments{ *this };
		jni::method<jfloat, ONESTEP_MC_NAME("method_7924"), jni::NOT_STATIC, BlockState> getMiningSpeedMultiplier{ *this };
		jni::method<jboolean, ONESTEP_MC_NAME("method_7951"), jni::NOT_STATIC, BlockState> isSuitableFor{ *this };
		jni::method<Object, bindings_profile::component, jni::NOT_STATIC, ComponentType> getComponent{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_31577"), jni::STATIC, ItemStack, ItemStack> areItemsAndComponentsEqual{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(ScreenHandler, ONESTEP_MC_NAME("net/minecraft/class_1703"))
        jni::field<jint, ONESTEP_MC_NAME("field_7763")> syncId{ *this };
        jni::field<DefaultedList, ONESTEP_MC_NAME("field_7761")> slots{ *this };
        jni::method<ItemStack, ONESTEP_MC_NAME("method_34255")> getCursorStack{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(UpdateSelectedSlotC2SPacket, ONESTEP_MC_NAME("net/minecraft/class_2868"))
        jni::constructor<jint> constructor{ *this };
    jni::method<jint, ONESTEP_MC_NAME("method_12442")> getSelectedSlot{ *this };
    jni::field<jint, ONESTEP_MC_NAME("field_13052")> selectedSlot{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(NbtCompound, ONESTEP_MC_NAME("net/minecraft/class_2487"))
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(DataComponentTypes, ONESTEP_MC_NAME("net/minecraft/class_9334"))
		jni::field<ComponentType, ONESTEP_MC_NAME("field_50075"), jni::STATIC> FOOD{ *this };
		jni::field<ComponentType, ONESTEP_MC_NAME("field_49651"), jni::STATIC> POTION_CONTENTS{ *this };
		jni::field<ComponentType, ONESTEP_MC_NAME("field_49649"), jni::STATIC> CHARGED_PROJECTILES{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(Element, ONESTEP_MC_NAME("net/minecraft/class_364"))
        jni::method<jboolean, ONESTEP_MC_NAME("method_25370")> isFocused{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(ParentElement, ONESTEP_MC_NAME("net/minecraft/class_4069"), Element)
        jni::method<List, ONESTEP_MC_NAME("method_25396")> children{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(TextFieldWidget, ONESTEP_MC_NAME("net/minecraft/class_342"), Element)
        jni::method<jboolean, ONESTEP_MC_NAME("method_1885")> isVisible{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_20315")> isActive{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(RecipeBookWidget, ONESTEP_MC_NAME("net/minecraft/class_507"))
        jni::field<TextFieldWidget, ONESTEP_MC_NAME("field_3089")> searchField{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_2605")> isOpen{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF(RecipeBookProvider, ONESTEP_MC_NAME("net/minecraft/class_518"))
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Screen, ONESTEP_MC_NAME("net/minecraft/class_437"))
		jni::field<jint, ONESTEP_MC_NAME("field_22789")> width{ *this };
		jni::field<jint, ONESTEP_MC_NAME("field_22790")> height{ *this };
        END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(RecipeBookScreen, ONESTEP_MC_NAME("net/minecraft/class_10260"), Screen)
        jni::field<RecipeBookWidget, ONESTEP_MC_NAME("field_54474")> recipeBook{*this};
    END_KLASS_DEF()
    inline RecipeBookWidget recipe_book(const Screen& screen)
    {
        return screen.is_instance_of<RecipeBookScreen>()
            ? RecipeBookScreen(screen.object_instance, false).recipeBook.get() : RecipeBookWidget{};
    }

        BEGIN_KLASS_DEF(Inventory, ONESTEP_MC_NAME("net/minecraft/class_1263"))
        jni::method<ItemStack, ONESTEP_MC_NAME("method_5438"), jni::NOT_STATIC, jint> getStack{ *this };
    jni::method<jint, ONESTEP_MC_NAME("method_5439")> size{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Slot, ONESTEP_MC_NAME("net/minecraft/class_1735"))
        jni::field<Inventory, ONESTEP_MC_NAME("field_7871")> inventory{ *this };
        jni::field<jint, ONESTEP_MC_NAME("field_7875")> index{ *this };
        jni::field<jint, ONESTEP_MC_NAME("field_7874")> id{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_7680"), jni::NOT_STATIC, ItemStack> canInsert{ *this };
        jni::method<ItemStack, ONESTEP_MC_NAME("method_7677")> getStack{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_7681")> hasStack{ *this };
        jni::method<jint, ONESTEP_MC_NAME("method_7676"), jni::NOT_STATIC, ItemStack> getMaxItemCount{ *this };
        jni::method<jboolean, ONESTEP_MC_NAME("method_7682")> isEnabled{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(HandledScreen, ONESTEP_MC_NAME("net/minecraft/class_465"), Screen)
        jni::field<Slot, ONESTEP_MC_NAME("field_2787")> focusedSlot{ *this };
        jni::field<ScreenHandler, ONESTEP_MC_NAME("field_2797")> handler{ *this };
    END_KLASS_DEF()

    BEGIN_KLASS_DEF_EX(InventoryScreen, ONESTEP_MC_NAME("net/minecraft/class_490"), Screen)
        END_KLASS_DEF()

	BEGIN_KLASS_DEF_EX(GenericContainerScreen, ONESTEP_MC_NAME("net/minecraft/class_476"), HandledScreen)
	END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(CreativeInventoryScreen, ONESTEP_MC_NAME("net/minecraft/class_481"), Screen)
        jni::field<TextFieldWidget, ONESTEP_MC_NAME("field_2894")> searchBox{ *this };
        END_KLASS_DEF()

        BEGIN_KLASS_DEF(Potion, ONESTEP_MC_NAME("net/minecraft/class_1842"))
        END_KLASS_DEF()

	BEGIN_KLASS_DEF(PotionContentsComponent, ONESTEP_MC_NAME("net/minecraft/class_1844"))
		jni::method<jboolean, ONESTEP_MC_NAME("method_57401"), jni::NOT_STATIC, RegistryEntry> matches{ *this };
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(Potions, ONESTEP_MC_NAME("net/minecraft/class_1847"))
#if ONESTEP_MINECRAFT_26_1
        // Holder.Reference<Potion> uses the same erased JNI class as this wrapper.
        jni::field<ItemRegistryEntry, ONESTEP_MC_NAME("field_8963"), jni::STATIC> healingReference{*this};
        jni::field<ItemRegistryEntry, ONESTEP_MC_NAME("field_8980"), jni::STATIC> strongHealingReference{*this};
        struct potion_reference
        {
            const Potions_members& owner;
            bool strong;
            bool is_valid() const { return strong ? owner.strongHealingReference.is_valid() : owner.healingReference.is_valid(); }
            RegistryEntry get() const
            {
                auto reference = strong ? owner.strongHealingReference.get() : owner.healingReference.get();
                return RegistryEntry(reference.object_instance, false);
            }
        } HEALING{*this, false}, STRONG_HEALING{*this, true};
#else
		jni::field<RegistryEntry, ONESTEP_MC_NAME("field_8963"), jni::STATIC> HEALING{ *this };
		jni::field<RegistryEntry, ONESTEP_MC_NAME("field_8980"), jni::STATIC> STRONG_HEALING{ *this };
#endif
    END_KLASS_DEF()

        BEGIN_KLASS_DEF(SlotActionType, ONESTEP_MC_NAME("net/minecraft/class_1713"))
        jni::field<SlotActionType, ONESTEP_MC_NAME("field_7790"), jni::STATIC> PICKUP{ *this };
        jni::field<SlotActionType, ONESTEP_MC_NAME("field_7794"), jni::STATIC> QUICK_MOVE{ *this };
        jni::field<SlotActionType, ONESTEP_MC_NAME("field_7791"), jni::STATIC> SWAP{ *this };
    END_KLASS_DEF()

        /* PotionItem. The previous class_1807 mapping
           made every real potion fail the runtime instanceof check. */
        BEGIN_KLASS_DEF_EX(PotionItem, ONESTEP_MC_NAME("net/minecraft/class_1812"), Item)
    END_KLASS_DEF()

        BEGIN_KLASS_DEF_EX(SplashPotionItem, ONESTEP_MC_NAME("net/minecraft/class_1828"), PotionItem)
    END_KLASS_DEF()



        BEGIN_KLASS_DEF(PlayerInventory, ONESTEP_MC_NAME("net/minecraft/class_1661"))
        jni::method<jint, ONESTEP_MC_NAME("method_7395"), jni::NOT_STATIC, ItemStack> getSlotWithStack{ *this };
    jni::method<void, ONESTEP_MC_NAME("method_7365"), jni::NOT_STATIC, jint> swapSlotWithHotbar{ *this };
    jni::field<DefaultedList, ONESTEP_MC_NAME("field_7547")> main{ *this };
	/* PlayerInventory.armor. */
    jni::field<jint, ONESTEP_MC_NAME("field_7545")> selectedSlot{ *this };
        ItemStack armor_stack(int index) const
        {
            if (index < 0 || index >= 4 || !object_instance) return {};
            // PlayerInventory's Inventory contract retains equipment indices.
            return Inventory(object_instance, false).getStack.call(36 + index);
        }
        ItemStack offhand_stack() const
        {
            return object_instance ? Inventory(object_instance, false).getStack.call(40) : ItemStack{};
        }
    END_KLASS_DEF()

}
