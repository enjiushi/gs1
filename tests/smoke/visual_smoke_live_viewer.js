import * as THREE_NS from "https://unpkg.com/three@0.165.0/build/three.module.js";

(function () {
    const stageFrame = document.getElementById("stage-frame");
    const gameView = document.getElementById("game-view");
    const hudEyebrow = document.getElementById("hud-eyebrow");
    const hudTitle = document.getElementById("hud-title");
    const hudSubtitle = document.getElementById("hud-subtitle");
    const statusChip = document.getElementById("status-chip");
    const menuPanel = document.getElementById("menu-panel");
    const menuEyebrow = document.getElementById("menu-eyebrow");
    const menuTitle = document.getElementById("menu-title");
    const menuSubtitle = document.getElementById("menu-subtitle");
    const menuCopy = document.getElementById("menu-copy");
    const menuMeta = document.getElementById("menu-meta");
    const menuActions = document.getElementById("menu-actions");
    const selectionEyebrow = document.getElementById("selection-eyebrow");
    const selectionText = document.getElementById("selection-text");
    const selectionInventory = document.getElementById("selection-inventory");
    const storagePanel = document.getElementById("storage-panel");
    const storagePanelTitle = document.getElementById("storage-panel-title");
    const storagePanelSubtitle = document.getElementById("storage-panel-subtitle");
    const storagePanelBody = document.getElementById("storage-panel-body");
    const storagePanelClose = document.getElementById("storage-panel-close");
    const contextActions = document.getElementById("context-actions");
    const phoneLayer = document.getElementById("phone-layer");
    const phoneStatusTime = document.getElementById("phone-status-time");
    const phoneAppSubtitle = document.getElementById("phone-app-subtitle");
    const phoneScreenBody = document.getElementById("phone-screen-body");
    const actionProgress = document.getElementById("action-progress");
    const actionProgressLabel = document.getElementById("action-progress-label");
    const actionProgressTime = document.getElementById("action-progress-time");
    const actionProgressTitle = document.getElementById("action-progress-title");
    const actionProgressFill = document.getElementById("action-progress-fill");
    const actionProgressPercent = document.getElementById("action-progress-percent");
    const actionProgressTarget = document.getElementById("action-progress-target");
    const inventoryTooltip = document.getElementById("inventory-tooltip");
    const inventoryTooltipTitle = document.getElementById("inventory-tooltip-title");
    const inventoryTooltipMeta = document.getElementById("inventory-tooltip-meta");
    const tileContextMenu = document.getElementById("tile-context-menu");

    let latestState = null;
    let stateStream = null;
    let siteControlSendInFlight = false;
    let mapPickables = [];
    let latestPresentationSignature = "";
    let lastSentSiteControlSignature = "0";
    let currentSceneKind = "";
    let siteSceneCache = null;
    let selectedInventorySlotKey = "";
    let openedInventoryContainerKey = "";
    let inventoryPanelOpen = true;
    let phonePanelOpen = false;
    let lastOverlayAppState = "";
    let tileContextMenuState = null;
    let localActionProgressState = null;
    let viewerCompatibilityWarning = "";
    let animationTimeSeconds = 0;
    let rendererWidth = 0;
    let rendererHeight = 0;
    let cameraOrbitDragPointerId = null;
    let cameraOrbitDragButton = -1;
    let cameraOrbitDragLastClientX = 0;
    let cameraOrbitPointerLockActive = false;
    let pendingSiteOrbitGesture = null;
    const orbitMouseButton = 2;
    const orbitMouseButtonsMask = 2;
    const orbitDragThresholdPixels = 6;

    const moveAxes = {
        x: 0,
        y: 0
    };

    const keys = new Set();
    const raycaster = new THREE_NS.Raycaster();
    const pointer = new THREE_NS.Vector2();
    const animationClock = new THREE_NS.Clock();
    const cameraForwardOnGround = new THREE_NS.Vector3();
    const cameraRightOnGround = new THREE_NS.Vector3();
    const desiredMoveOnGround = new THREE_NS.Vector3();
    const worldUp = new THREE_NS.Vector3(0, 1, 0);
    const siteCameraDefaults = {
        offsetX: 4.6,
        offsetZ: 5.2,
        lookHeight: 0.78,
        viewAngleRadians: Math.PI / 4,
        nearestGroundDistance: 9.0,
        farthestGroundDistance: 18.0,
        wheelZoomStepDistance: 1.5,
        rotateRadiansPerPixel: 0.002
    };
    const runtimeFixedStepSeconds = 1.0 / 60.0;
    const siteActionMinutesPerRealSecond = 0.8;
    const craftCacheRadiusTiles = 5;
    const inventoryContainerCodes = {
        WORKER_PACK: 0,
        DEVICE_STORAGE: 1
    };
    let inventoryCache = createEmptyInventoryCache();
    const itemCatalog = {
        1: { name: "Water", shortName: "H2O", stackSize: 5, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        2: { name: "Food", shortName: "Ration", stackSize: 5, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        3: { name: "Medicine", shortName: "Med", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Use" },
        4: { name: "Wind Reed Seeds", shortName: "Wind Reed", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        5: { name: "Saltbush Seeds", shortName: "Saltbush", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        6: { name: "Shade Cactus Seeds", shortName: "Shade Cactus", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        7: { name: "Sunfruit Vine Seeds", shortName: "Sunfruit Vine", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        8: { name: "Wood", shortName: "Wood", stackSize: 20, canUse: false, canPlant: false, canDeploy: false },
        9: { name: "Iron", shortName: "Iron", stackSize: 20, canUse: false, canPlant: false, canDeploy: false },
        10: { name: "Wind Reed Fiber", shortName: "Fiber", stackSize: 20, canUse: false, canPlant: false, canDeploy: false },
        11: { name: "Camp Stove Kit", shortName: "Stove Kit", stackSize: 1, canUse: false, canPlant: false, canDeploy: true, deployStructureId: 201 },
        12: { name: "Workbench Kit", shortName: "Bench Kit", stackSize: 1, canUse: false, canPlant: false, canDeploy: true, deployStructureId: 202 },
        13: { name: "Storage Crate Kit", shortName: "Crate Kit", stackSize: 1, canUse: false, canPlant: false, canDeploy: true, deployStructureId: 203 },
        14: { name: "Hammer", shortName: "Hammer", stackSize: 1, canUse: false, canPlant: false, canDeploy: false }
    };
    const itemVisuals = {
        1: { glyph: "H2", light: "#5d8eb3", dark: "#33546f" },
        2: { glyph: "FD", light: "#b48a4f", dark: "#6f4a28" },
        3: { glyph: "MD", light: "#9a676c", dark: "#62363b" },
        4: { glyph: "WR", light: "#79a35f", dark: "#46693a" },
        5: { glyph: "SB", light: "#8aa05c", dark: "#53683a" },
        6: { glyph: "SC", light: "#6a8c67", dark: "#3c5841" },
        7: { glyph: "SV", light: "#8f7c4c", dark: "#5c4526" },
        8: { glyph: "WD", light: "#ac7d4b", dark: "#6e4a28" },
        9: { glyph: "IR", light: "#9ca7b4", dark: "#596572" },
        10: { glyph: "FB", light: "#a2bb7b", dark: "#607541" },
        11: { glyph: "ST", light: "#c7835a", dark: "#7b4327" },
        12: { glyph: "WB", light: "#b2966c", dark: "#6b5437" },
        13: { glyph: "CR", light: "#af8c63", dark: "#694d2c" },
        14: { glyph: "HM", light: "#9ea9b7", dark: "#5c6775" }
    };
    const structureCatalog = {
        201: { name: "Camp Stove", shortName: "Stove", slotCount: 6 },
        202: { name: "Workbench", shortName: "Workbench", slotCount: 8 },
        203: { name: "Storage Crate", shortName: "Crate", slotCount: 10 }
    };
    const craftRecipeCatalog = {
        201: [
            {
                outputItemId: 2,
                outputQuantity: 1,
                craftMinutes: 1.0,
                ingredients: [
                    { itemId: 1, quantity: 1 },
                    { itemId: 10, quantity: 2 }
                ]
            }
        ],
        202: [
            {
                outputItemId: 11,
                outputQuantity: 1,
                craftMinutes: 1.5,
                ingredients: [
                    { itemId: 8, quantity: 4 },
                    { itemId: 9, quantity: 2 }
                ]
            },
            {
                outputItemId: 12,
                outputQuantity: 1,
                craftMinutes: 2.0,
                ingredients: [
                    { itemId: 8, quantity: 5 },
                    { itemId: 9, quantity: 3 }
                ]
            },
            {
                outputItemId: 13,
                outputQuantity: 1,
                craftMinutes: 1.0,
                ingredients: [
                    { itemId: 8, quantity: 3 },
                    { itemId: 9, quantity: 2 }
                ]
            },
            {
                outputItemId: 14,
                outputQuantity: 1,
                craftMinutes: 1.0,
                ingredients: [
                    { itemId: 8, quantity: 2 },
                    { itemId: 9, quantity: 1 }
                ]
            }
        ]
    };

    const renderer = new THREE_NS.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.outputColorSpace = THREE_NS.SRGBColorSpace;
    renderer.domElement.style.touchAction = "none";
    gameView.appendChild(renderer.domElement);

    const scene = new THREE_NS.Scene();
    scene.background = new THREE_NS.Color(0xf4e6cf);

    const camera = new THREE_NS.PerspectiveCamera(45, 1, 0.1, 2000);
    const ambient = new THREE_NS.AmbientLight(0xffffff, 1.3);
    const directional = new THREE_NS.DirectionalLight(0xffffff, 1.0);
    directional.position.set(12, 18, 10);
    scene.add(ambient, directional);

    const worldGroup = new THREE_NS.Group();
    scene.add(worldGroup);

    function fitRenderer() {
        const width = Math.max(gameView.clientWidth, 320);
        const height = Math.max(gameView.clientHeight, 240);
        if (width === rendererWidth && height === rendererHeight) {
            return;
        }

        rendererWidth = width;
        rendererHeight = height;
        renderer.setSize(width, height, false);
        camera.aspect = width / height;
        camera.updateProjectionMatrix();
    }

    function disposeObject(object) {
        object.traverse((node) => {
            if (node.geometry) {
                node.geometry.dispose();
            }
            if (Array.isArray(node.material)) {
                node.material.forEach((material) => {
                    if (material && typeof material.dispose === "function") {
                        material.dispose();
                    }
                });
            } else if (node.material && typeof node.material.dispose === "function") {
                node.material.dispose();
            }
        });
    }

    function clearWorld() {
        stopCameraOrbitDrag();
        closeTileContextMenu();
        while (worldGroup.children.length > 0) {
            const child = worldGroup.children[0];
            worldGroup.remove(child);
            disposeObject(child);
        }
        mapPickables = [];
        siteSceneCache = null;
    }

    function postJson(url, payload) {
        return fetch(url, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });
    }

    function makeButton(label, onClick, secondary, disabled) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "action-button" + (secondary ? " secondary" : "");
        button.textContent = label;
        button.disabled = !!disabled;
        button.addEventListener("click", onClick);
        return button;
    }

    function makePhoneActionButton(label, onClick, secondary, disabled) {
        const button = makeButton(label, onClick, secondary, disabled);
        button.classList.add("phone-action-button");
        return button;
    }

    function getSetup(state, setupId) {
        return (state.uiSetups || []).find((setup) => setup.setupId === setupId) || null;
    }

    function getActionElements(setup) {
        if (!setup) {
            return [];
        }
        return setup.elements.filter((element) => element.action && element.action.type !== "NONE");
    }

    function getVisibleActionElements(setup) {
        return getActionElements(setup).filter((element) => (element.flags & 4) === 0);
    }

    function getBackgroundClickElement(setup) {
        return getActionElements(setup).find((element) => (element.flags & 4) !== 0) || null;
    }

    function getLabelElements(setup) {
        if (!setup) {
            return [];
        }
        return setup.elements.filter((element) => !element.action || element.action.type === "NONE");
    }

    function getSiteBootstrap(state) {
        return state.siteBootstrap || null;
    }

    function getSiteState(state) {
        return state.siteState || null;
    }

    function createEmptyInventoryCache() {
        return {
            frameNumber: 0,
            slots: [],
            slotMap: new Map(),
            containers: [],
            containerMap: new Map()
        };
    }

    function rebuildInventoryCache(state) {
        const nextCache = createEmptyInventoryCache();
        if (!state || state.appState !== "SITE_ACTIVE") {
            inventoryCache = nextCache;
            return;
        }

        const siteState = getSiteState(state);
        const sourceStorages = siteState && Array.isArray(siteState.inventoryStorages)
            ? siteState.inventoryStorages
            : [];
        const workerPackSlots = siteState && Array.isArray(siteState.workerPackSlots)
            ? siteState.workerPackSlots
            : [];
        const openedStorageSlots =
            siteState && siteState.openedStorage && Array.isArray(siteState.openedStorage.slots)
                ? siteState.openedStorage.slots
                : [];
        const sourceSlots = workerPackSlots.concat(openedStorageSlots);
        nextCache.frameNumber = typeof state.frameNumber === "number" ? state.frameNumber : 0;

        sourceStorages.forEach((storage) => {
            const storageId = typeof storage.storageId === "number" ? storage.storageId : 0;
            const cachedContainerKey = containerKey(
                storage.containerKind || "WORKER_PACK",
                storageId
            );
            nextCache.containerMap.set(cachedContainerKey, {
                key: cachedContainerKey,
                containerKind: storage.containerKind || "WORKER_PACK",
                containerOwnerId: storageId,
                storageId: storageId,
                ownerEntityId: typeof storage.ownerEntityId === "number" ? storage.ownerEntityId : 0,
                containerTileX: typeof storage.tileX === "number" ? storage.tileX : 0,
                containerTileY: typeof storage.tileY === "number" ? storage.tileY : 0,
                slotCount: typeof storage.slotCount === "number" ? storage.slotCount : 0,
                isOpenedView:
                    !!(siteState &&
                        siteState.openedStorage &&
                        typeof siteState.openedStorage.storageId === "number" &&
                        siteState.openedStorage.storageId === storageId),
                slots: []
            });
        });

        sourceSlots.forEach((sourceSlot) => {
            const normalizedSlot = {
                containerKind: sourceSlot.containerKind || "WORKER_PACK",
                slotIndex: typeof sourceSlot.slotIndex === "number" ? sourceSlot.slotIndex : 0,
                itemInstanceId: typeof sourceSlot.itemInstanceId === "number" ? sourceSlot.itemInstanceId : 0,
                itemId: typeof sourceSlot.itemId === "number" ? sourceSlot.itemId : 0,
                itemName: sourceSlot.itemName || null,
                storageId: typeof sourceSlot.storageId === "number" ? sourceSlot.storageId : 0,
                containerOwnerId: typeof sourceSlot.storageId === "number" ? sourceSlot.storageId : 0,
                quantity: typeof sourceSlot.quantity === "number" ? sourceSlot.quantity : 0,
                condition: typeof sourceSlot.condition === "number" ? sourceSlot.condition : 0,
                freshness: typeof sourceSlot.freshness === "number" ? sourceSlot.freshness : 0,
                containerTileX: typeof sourceSlot.containerTileX === "number" ? sourceSlot.containerTileX : 0,
                containerTileY: typeof sourceSlot.containerTileY === "number" ? sourceSlot.containerTileY : 0,
                flags: typeof sourceSlot.flags === "number" ? sourceSlot.flags : 0
            };

            const cachedSlotKey = slotKey(
                normalizedSlot.containerKind,
                normalizedSlot.slotIndex,
                normalizedSlot.storageId
            );
            nextCache.slots.push(normalizedSlot);
            nextCache.slotMap.set(cachedSlotKey, normalizedSlot);

            const cachedContainerKey = containerKey(
                normalizedSlot.containerKind,
                normalizedSlot.storageId
            );
            if (!nextCache.containerMap.has(cachedContainerKey)) {
                nextCache.containerMap.set(cachedContainerKey, {
                    key: cachedContainerKey,
                    containerKind: normalizedSlot.containerKind,
                    containerOwnerId: normalizedSlot.storageId,
                    storageId: normalizedSlot.storageId,
                    ownerEntityId: typeof sourceSlot.containerOwnerId === "number" ? sourceSlot.containerOwnerId : 0,
                    containerTileX: normalizedSlot.containerTileX,
                    containerTileY: normalizedSlot.containerTileY,
                    slotCount:
                        normalizedSlot.containerKind === "WORKER_PACK"
                            ? workerPackSlots.length
                            : (siteState && siteState.openedStorage && siteState.openedStorage.storageId === normalizedSlot.storageId
                                ? (siteState.openedStorage.slotCount || 0)
                                : 0),
                    isOpenedView:
                        !!(siteState &&
                            siteState.openedStorage &&
                            siteState.openedStorage.storageId === normalizedSlot.storageId),
                    slots: []
                });
            }

            nextCache.containerMap.get(cachedContainerKey).slots.push(normalizedSlot);
        });

        nextCache.containers = Array.from(nextCache.containerMap.values()).sort((left, right) => {
            if (left.containerKind !== right.containerKind) {
                return left.containerKind.localeCompare(right.containerKind);
            }
            if ((left.storageId || 0) !== (right.storageId || 0)) {
                return (left.storageId || 0) - (right.storageId || 0);
            }
            return 0;
        });
        nextCache.containers.forEach((containerInfo) => {
            containerInfo.slots.sort((left, right) => left.slotIndex - right.slotIndex);
        });

        inventoryCache = nextCache;
    }

    function getHudState(state) {
        return state ? state.hud || null : null;
    }

    function getItemMeta(itemId) {
        return itemCatalog[itemId] || null;
    }

    function getItemLabel(itemId) {
        const itemMeta = getItemMeta(itemId);
        return itemMeta ? itemMeta.name : ("Item " + itemId);
    }

    function getInventoryItemLabel(slot) {
        if (slot && slot.itemName) {
            return slot.itemName;
        }
        return getItemLabel(slot ? slot.itemId : 0);
    }

    function getItemShortLabel(itemId) {
        const itemMeta = getItemMeta(itemId);
        return itemMeta ? itemMeta.shortName : ("Item " + itemId);
    }

    function getItemVisual(itemId) {
        return itemVisuals[itemId] || { glyph: "??", light: "#8f7a64", dark: "#5a4837" };
    }

    function findItemIdByLabel(labelText) {
        const target = String(labelText || "").trim();
        if (!target) {
            return 0;
        }

        const itemIds = Object.keys(itemCatalog);
        for (let index = 0; index < itemIds.length; index += 1) {
            const itemId = Number(itemIds[index]);
            if (getItemLabel(itemId) === target) {
                return itemId;
            }
        }

        return 0;
    }

    function getStructureMeta(structureId) {
        return structureCatalog[structureId] || null;
    }

    function getStructureLabel(structureId) {
        const structureMeta = getStructureMeta(structureId);
        return structureMeta ? structureMeta.name : ("Structure " + structureId);
    }

    function structureProvidesStorage(structureId) {
        const structureMeta = getStructureMeta(structureId);
        return !!(structureMeta && (structureMeta.slotCount || 0) > 0);
    }

    function getCraftRecipesForStructure(structureId) {
        return craftRecipeCatalog[structureId] || [];
    }

    function getVisualSmokeCompatibilityWarning(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            return "";
        }

        const siteState = getSiteState(state);
        if (!siteState) {
            return "";
        }

        if (Array.isArray(siteState.inventoryStorages) && Array.isArray(siteState.workerPackSlots)) {
            return "";
        }

        if (Array.isArray(siteState.inventorySlots)) {
            return "Visual Smoke Build Mismatch\nOpen Storage unavailable\nRebuild gs1_visual_smoke_host\nRebuild gs1_game.dll";
        }

        return "Visual Smoke Host Error\nInventory projection missing\nRebuild gs1_visual_smoke_host\nReload this page";
    }

    function getTileSnapshot(state, tileX, tileY) {
        const siteBootstrap = getSiteBootstrap(state);
        if (!siteBootstrap || !siteBootstrap.tiles) {
            return null;
        }

        return siteBootstrap.tiles.find((tile) => tile.x === tileX && tile.y === tileY) || null;
    }

    function getAllInventorySlots(state) {
        return inventoryCache.slots.slice();
    }

    function getInventorySlotsByKind(state, containerKind) {
        return getAllInventorySlots(state).filter((slot) => slot.containerKind === containerKind);
    }

    function getCarriedItemQuantity(state, itemId) {
        let total = 0;
        getInventorySlotsByKind(state, "WORKER_PACK").forEach((slot) => {
            if (!isOccupiedSlot(slot) || slot.itemId !== itemId) {
                return;
            }

            total += slot.quantity;
        });
        return total;
    }

    function getBuyListings(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.phoneListings) {
            return [];
        }

        return siteState.phoneListings.filter((listing) => listing.listingKind === "BUY_ITEM" && listing.quantity !== 0);
    }

    function getSellListings(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.phoneListings) {
            return [];
        }

        return siteState.phoneListings.filter((listing) => listing.listingKind === "SELL_ITEM" && listing.quantity !== 0);
    }

    function getSpecialPhoneListings(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.phoneListings) {
            return [];
        }

        return siteState.phoneListings.filter((listing) =>
            listing.listingKind !== "BUY_ITEM" &&
            listing.listingKind !== "SELL_ITEM" &&
            listing.quantity !== 0);
    }

    function getSiteTasks(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.tasks) {
            return [];
        }

        return siteState.tasks.slice();
    }

    function getCraftContextForTile(state, tileX, tileY) {
        const siteState = getSiteState(state);
        const craftContext = siteState ? siteState.craftContext : null;
        if (!craftContext) {
            return null;
        }
        if ((craftContext.tileX || 0) !== tileX || (craftContext.tileY || 0) !== tileY) {
            return null;
        }
        return craftContext;
    }

    function containerKey(containerKind, containerOwnerId) {
        return containerKind + ":" + String(containerOwnerId || 0);
    }

    function slotKey(containerKind, slotIndex, containerOwnerId) {
        return containerKey(containerKind, containerOwnerId) + ":" + slotIndex;
    }

    function encodeInventoryTransferOwnersArg(sourceOwnerId, destinationOwnerId) {
        return (sourceOwnerId >>> 0) + ((destinationOwnerId >>> 0) * 0x100000000);
    }

    function encodeInventoryUseArg(containerKind, slotIndex, quantity) {
        const containerCode = inventoryContainerCodes[containerKind] || 0;
        return containerCode + ((slotIndex & 0xff) << 8) + ((quantity & 0xffff) << 16);
    }

    function encodeInventoryTransferArg(
        sourceContainerKind,
        sourceSlotIndex,
        destinationContainerKind,
        quantity
    ) {
        const sourceContainerCode = inventoryContainerCodes[sourceContainerKind] || 0;
        const destinationContainerCode = inventoryContainerCodes[destinationContainerKind] || 0;
        return sourceContainerCode +
            ((sourceSlotIndex & 0xff) << 8) +
            ((destinationContainerCode & 0xff) << 16) +
            ((quantity & 0xffff) * 0x1000000);
    }

    function getInventoryContainers(state) {
        return inventoryCache.containers.map((containerInfo) => ({
            key: containerInfo.key,
            containerKind: containerInfo.containerKind,
            containerOwnerId: containerInfo.containerOwnerId,
            storageId: containerInfo.storageId,
            ownerEntityId: containerInfo.ownerEntityId,
            containerTileX: containerInfo.containerTileX,
            containerTileY: containerInfo.containerTileY,
            slotCount: containerInfo.slotCount,
            isOpenedView: !!containerInfo.isOpenedView,
            slots: containerInfo.slots.slice()
        }));
    }

    function findInventoryContainerByKey(containerLookupKey) {
        if (!containerLookupKey) {
            return null;
        }

        return inventoryCache.containerMap.get(containerLookupKey) || null;
    }

    function getDeviceStorageContainers(state) {
        return getInventoryContainers(state).filter((container) => container.containerKind === "DEVICE_STORAGE");
    }

    function getOpenableStorageContainersForTile(state, tileX, tileY) {
        return getInventoryContainers(state).filter((container) =>
            container.containerKind !== "WORKER_PACK" &&
            container.containerTileX === tileX &&
            container.containerTileY === tileY);
    }

    function findOpenedInventoryContainer(state) {
        if (!openedInventoryContainerKey) {
            return null;
        }

        return findInventoryContainerByKey(openedInventoryContainerKey);
    }

    function clearOpenedInventoryContainerIfInvalid(state) {
        if (!openedInventoryContainerKey || !state || state.appState !== "SITE_ACTIVE") {
            openedInventoryContainerKey = "";
            return;
        }

        if (!findOpenedInventoryContainer(state)) {
            openedInventoryContainerKey = "";
        }
    }

    function openInventoryContainer(containerInfo) {
        openedInventoryContainerKey = containerInfo ? containerInfo.key : "";
        if (containerInfo && typeof containerInfo.storageId === "number") {
            postJson("/site-storage-view", {
                storageId: containerInfo.storageId,
                eventKind: "OPEN_SNAPSHOT"
            }).catch(() => {
                statusChip.textContent = "Failed to open storage.";
            });
        }
        if (latestState) {
            renderSiteOverlay(latestState);
        }
    }

    function closeOpenedInventoryContainer() {
        const openedContainer = latestState ? findOpenedInventoryContainer(latestState) : null;
        const closedContainerKey = openedInventoryContainerKey;
        openedInventoryContainerKey = "";
        if (closedContainerKey && selectedInventorySlotKey.startsWith(closedContainerKey + ":")) {
            selectedInventorySlotKey = "";
        }
        if (openedContainer && typeof openedContainer.storageId === "number") {
            postJson("/site-storage-view", {
                storageId: openedContainer.storageId,
                eventKind: "CLOSE"
            }).catch(() => {
                statusChip.textContent = "Failed to close storage.";
            });
        }
        if (latestState) {
            renderSiteOverlay(latestState);
        }
    }

    function getContainerDisplayName(state, containerInfo) {
        if (!containerInfo) {
            return "Storage";
        }
        if (containerInfo.containerKind === "WORKER_PACK") {
            return "Worker Pack";
        }

        const tile = getTileSnapshot(state, containerInfo.containerTileX, containerInfo.containerTileY);
        const structureId = tile ? tile.structureTypeId : 0;
        const structureLabel = getStructureLabel(structureId);
        return structureLabel + " (" + containerInfo.containerTileX + ", " + containerInfo.containerTileY + ")";
    }

    function getCarriedSeedOptions(state) {
        const seedTotals = new Map();
        const workerPackSlots = getInventorySlotsByKind(state, "WORKER_PACK");
        workerPackSlots.forEach((slot) => {
            if (!isOccupiedSlot(slot)) {
                return;
            }

            const itemMeta = getItemMeta(slot.itemId);
            if (!itemMeta || !itemMeta.canPlant) {
                return;
            }

            const previousQuantity = seedTotals.get(slot.itemId) || 0;
            seedTotals.set(slot.itemId, previousQuantity + slot.quantity);
        });

        return Array.from(seedTotals.entries())
            .sort((lhs, rhs) => lhs[0] - rhs[0])
            .map(([itemId, quantity]) => {
                const itemMeta = getItemMeta(itemId);
                const visual = getItemVisual(itemId);
                return {
                    id: "seed:" + itemId,
                    itemId: itemId,
                    label: itemMeta ? itemMeta.name : ("Item " + itemId),
                    shortLabel: itemMeta ? itemMeta.shortName : ("Item " + itemId),
                    quantity: quantity,
                    iconGlyph: visual.glyph,
                    iconLight: visual.light,
                    iconDark: visual.dark
                };
            });
    }

    function getCarriedDeployableOptions(state) {
        const deployTotals = new Map();
        getInventorySlotsByKind(state, "WORKER_PACK").forEach((slot) => {
            if (!isOccupiedSlot(slot)) {
                return;
            }

            const itemMeta = getItemMeta(slot.itemId);
            if (!itemMeta || !itemMeta.canDeploy || !itemMeta.deployStructureId) {
                return;
            }

            const previous = deployTotals.get(slot.itemId) || 0;
            deployTotals.set(slot.itemId, previous + slot.quantity);
        });

        return Array.from(deployTotals.entries())
            .sort((lhs, rhs) => lhs[0] - rhs[0])
            .map(([itemId, quantity]) => {
                const itemMeta = getItemMeta(itemId);
                const visual = getItemVisual(itemId);
                return {
                    id: "deploy:" + itemId,
                    itemId: itemId,
                    label: itemMeta ? itemMeta.name : ("Item " + itemId),
                    shortLabel: itemMeta ? itemMeta.shortName : ("Item " + itemId),
                    quantity: quantity,
                    iconGlyph: visual.glyph,
                    iconLight: visual.light,
                    iconDark: visual.dark
                };
            });
    }

    function isWithinCraftRange(tileX, tileY, otherTileX, otherTileY) {
        return Math.abs((otherTileX || 0) - tileX) <= craftCacheRadiusTiles &&
            Math.abs((otherTileY || 0) - tileY) <= craftCacheRadiusTiles;
    }

    function getNearbyCraftSourceSlots(state, tileX, tileY) {
        const siteState = getSiteState(state);
        const worker = siteState ? siteState.worker : null;
        return getAllInventorySlots(state).filter((slot) => {
            if (!isOccupiedSlot(slot)) {
                return false;
            }
            if (slot.containerKind === "WORKER_PACK") {
                if (!worker) {
                    return false;
                }
                return isWithinCraftRange(tileX, tileY, Math.round(worker.tileX || 0), Math.round(worker.tileY || 0));
            }
            return isWithinCraftRange(tileX, tileY, slot.containerTileX || 0, slot.containerTileY || 0);
        });
    }

    function summarizeItemTotals(slots) {
        const totals = new Map();
        slots.forEach((slot) => {
            if (!isOccupiedSlot(slot)) {
                return;
            }
            totals.set(slot.itemId, (totals.get(slot.itemId) || 0) + slot.quantity);
        });
        return totals;
    }

    function canCraftRecipeFromNearby(state, tileX, tileY, recipe) {
        const nearbyTotals = summarizeItemTotals(getNearbyCraftSourceSlots(state, tileX, tileY));
        return recipe.ingredients.every((ingredient) => (nearbyTotals.get(ingredient.itemId) || 0) >= ingredient.quantity);
    }

    function clearSelectedInventorySlotIfInvalid(state) {
        if (!selectedInventorySlotKey || !state || state.appState !== "SITE_ACTIVE") {
            selectedInventorySlotKey = "";
            return;
        }

        const selectedSlot = getAllInventorySlots(state).find((slot) =>
            slotKey(slot.containerKind, slot.slotIndex, slot.containerOwnerId) === selectedInventorySlotKey);
        if (!selectedSlot || selectedSlot.quantity <= 0 || selectedSlot.flags === 0) {
            selectedInventorySlotKey = "";
        }
    }

    function findSelectedInventorySlot(state) {
        if (!selectedInventorySlotKey || !state) {
            return null;
        }
        return getAllInventorySlots(state).find((slot) =>
            slotKey(slot.containerKind, slot.slotIndex, slot.containerOwnerId) === selectedInventorySlotKey) || null;
    }

    function findWorkerPackInventoryContainer(state) {
        return getInventoryContainers(state).find((container) => container.containerKind === "WORKER_PACK") || null;
    }

    function slotUsesPrimaryTransferClick(state, sourceSlot) {
        if (!state || state.appState !== "SITE_ACTIVE" || !isOccupiedSlot(sourceSlot)) {
            return false;
        }

        if (sourceSlot.containerKind === "WORKER_PACK") {
            const openedContainer = findOpenedInventoryContainer(state);
            return !!(openedContainer && openedContainer.containerKind === "DEVICE_STORAGE");
        }

        if (sourceSlot.containerKind === "DEVICE_STORAGE") {
            const openedContainer = findOpenedInventoryContainer(state);
            return !!openedContainer &&
                openedContainer.containerKind === sourceSlot.containerKind &&
                (openedContainer.containerOwnerId || 0) === (sourceSlot.containerOwnerId || 0);
        }

        return false;
    }

    function findPrimaryTransferTargetContainer(state, sourceSlot) {
        if (!slotUsesPrimaryTransferClick(state, sourceSlot)) {
            return null;
        }

        if (sourceSlot.containerKind === "WORKER_PACK") {
            return findOpenedInventoryContainer(state);
        }

        if (sourceSlot.containerKind === "DEVICE_STORAGE") {
            return findWorkerPackInventoryContainer(state);
        }

        return null;
    }

    function slotSupportsSelection(slot) {
        if (!isOccupiedSlot(slot) || slot.containerKind !== "WORKER_PACK") {
            return false;
        }

        const itemMeta = getItemMeta(slot.itemId);
        return !!(itemMeta && itemMeta.canUse);
    }

    function postInventoryUse(slot) {
        return postJson("/ui-action", {
            type: "USE_INVENTORY_ITEM",
            targetId: slot.itemInstanceId || slot.itemId,
            arg0: encodeInventoryUseArg(slot.containerKind, slot.slotIndex, 1),
            arg1: slot.storageId || 0
        });
    }

    function postInventoryTransfer(sourceSlot, destinationKind, destinationOwnerId) {
        return postJson("/ui-action", {
            type: "TRANSFER_INVENTORY_ITEM",
            targetId: sourceSlot.itemInstanceId || sourceSlot.itemId,
            arg0: encodeInventoryTransferArg(
                sourceSlot.containerKind,
                sourceSlot.slotIndex,
                destinationKind,
                0
            ),
            arg1: encodeInventoryTransferOwnersArg(
                sourceSlot.storageId || 0,
                destinationOwnerId || 0
            )
        });
    }

    function postSiteAction(payload) {
        return postJson("/site-action", payload);
    }

    function postSiteContextRequest(tileX, tileY) {
        return postJson("/site-context", {
            tileX: tileX,
            tileY: tileY,
            flags: 0
        });
    }

    function getSiteActionLabel(actionKind) {
        if (actionKind === 1) {
            return "Plant";
        }
        if (actionKind === 2) {
            return "Build";
        }
        if (actionKind === 3) {
            return "Repair";
        }
        if (actionKind === 4) {
            return "Water";
        }
        if (actionKind === 5) {
            return "Clear Burial";
        }
        if (actionKind === 6) {
            return "Craft";
        }
        if (actionKind === 7) {
            return "Drink";
        }
        if (actionKind === 8) {
            return "Eat";
        }
        return "Idle";
    }

    function formatActionSeconds(totalSeconds) {
        if (totalSeconds >= 60) {
            return (totalSeconds / 60).toFixed(1) + "m";
        }
        return totalSeconds.toFixed(1) + "s";
    }

    function syncLocalActionProgress(state) {
        const siteAction = state ? state.siteAction : null;
        const nowMs = performance.now();

        if (!siteAction || (siteAction.flags & 1) === 0 || siteAction.actionKind === 0) {
            localActionProgressState = null;
            return;
        }

        const durationMinutes = Math.max(siteAction.durationMinutes || 0, 0);
        const durationMs = Math.max((durationMinutes / siteActionMinutesPerRealSecond) * 1000, 1);
        const clampedProgress = Math.max(0, Math.min(siteAction.progressNormalized || 0, 1));

        if (localActionProgressState &&
            localActionProgressState.actionId === siteAction.actionId &&
            localActionProgressState.durationMs === durationMs) {
            localActionProgressState.lastSyncProgress = clampedProgress;
            return;
        }

        localActionProgressState = {
            actionId: siteAction.actionId,
            actionKind: siteAction.actionKind,
            targetTileX: siteAction.targetTileX,
            targetTileY: siteAction.targetTileY,
            durationMs: durationMs,
            startedAtMs: nowMs - clampedProgress * durationMs,
            lastSyncProgress: clampedProgress
        };
    }

    function renderActionProgressBar(state) {
        if (!actionProgress) {
            return;
        }

        if (!state || state.appState !== "SITE_ACTIVE" || !localActionProgressState) {
            actionProgress.hidden = true;
            return;
        }

        const nowMs = performance.now();
        const elapsedMs = Math.max(0, nowMs - localActionProgressState.startedAtMs);
        const progress = Math.max(0, Math.min(elapsedMs / localActionProgressState.durationMs, 1));
        const remainingSeconds = Math.max(0, (localActionProgressState.durationMs - elapsedMs) / 1000);

        actionProgress.hidden = false;
        actionProgressLabel.textContent = "Current Action";
        actionProgressTitle.textContent = getSiteActionLabel(localActionProgressState.actionKind);
        actionProgressTime.textContent = formatActionSeconds(remainingSeconds);
        actionProgressFill.style.width = (progress * 100).toFixed(1) + "%";
        actionProgressPercent.textContent = Math.round(progress * 100) + "%";
        actionProgressTarget.textContent =
            localActionProgressState.actionKind === 7 || localActionProgressState.actionKind === 8
                ? "Self"
                : ("Tile " + localActionProgressState.targetTileX + ", " + localActionProgressState.targetTileY);
    }

    function renderSiteStatusChip(state) {
        if (viewerCompatibilityWarning) {
            statusChip.textContent = viewerCompatibilityWarning;
            return;
        }

        const siteState = getSiteState(state);
        const hud = getHudState(state);
        const weather = siteState ? siteState.weather : null;
        const carriedSeeds = getCarriedSeedOptions(state);
        const workerActionKind =
            siteState && siteState.worker ? siteState.worker.currentActionKind : 0;
        const hydration = hud ? Math.round(hud.playerHydration) : 0;
        const energy = hud ? Math.round(hud.playerEnergy) : 0;
        const completion = hud ? Math.round((hud.siteCompletionNormalized || 0) * 100) : 0;
        const eventPhase = weather ? weather.eventPhase : "NONE";

        statusChip.textContent =
            "Site Live\nHydration " + hydration +
            "\nEnergy " + energy +
            "\nCompletion " + completion + "%" +
            "\nEvent " + eventPhase +
            "\nSeeds " + carriedSeeds.reduce((total, seed) => total + seed.quantity, 0) +
            "\nAction " + getSiteActionLabel(workerActionKind);
    }

    function pickSiteTileAtClientPosition(clientX, clientY) {
        if (!latestState || latestState.appState !== "SITE_ACTIVE" || !siteSceneCache || !siteSceneCache.tilePickables) {
            return null;
        }

        const rect = renderer.domElement.getBoundingClientRect();
        pointer.x = ((clientX - rect.left) / Math.max(rect.width, 1)) * 2 - 1;
        pointer.y = -(((clientY - rect.top) / Math.max(rect.height, 1)) * 2 - 1);
        raycaster.setFromCamera(pointer, camera);

        const hits = raycaster.intersectObjects(siteSceneCache.tilePickables, false);
        if (hits.length === 0) {
            return null;
        }

        return hits[0].object.userData || null;
    }

    function closeTileContextMenu() {
        tileContextMenuState = null;
        tileContextMenu.hidden = true;
        tileContextMenu.innerHTML = "";
    }

    function setTileContextMenuHover(level, itemId, expands) {
        if (!tileContextMenuState) {
            return;
        }

        const nextHoverPath = tileContextMenuState.hoverPath.slice(0, level);
        if (expands) {
            nextHoverPath[level] = itemId;
        }
        if (nextHoverPath.length === tileContextMenuState.hoverPath.length &&
            nextHoverPath.every(function (hoveredId, index) {
                return hoveredId === tileContextMenuState.hoverPath[index];
            })) {
            return;
        }
        tileContextMenuState.hoverPath = nextHoverPath;
        renderTileContextMenu();
    }

    function collectTileContextPanels(rootItems, hoverPath) {
        const panels = [];
        let items = rootItems;
        let level = 0;

        while (items && items.length > 0) {
            panels.push({ level: level, items: items });
            const hoveredId = hoverPath[level];
            const hoveredItem = items.find((item) => item.id === hoveredId);
            if (!hoveredItem || !hoveredItem.children || hoveredItem.children.length === 0) {
                break;
            }

            items = hoveredItem.children;
            level += 1;
        }

        return panels;
    }

    function buildTileContextMenuItems(state, tileX, tileY) {
        const tile = getTileSnapshot(state, tileX, tileY);
        const structureId = tile ? (tile.structureTypeId || 0) : 0;
        const recipes = getCraftRecipesForStructure(structureId);
        const craftContext = getCraftContextForTile(state, tileX, tileY);
        const storageContainers = getOpenableStorageContainersForTile(state, tileX, tileY);
        const carriedSeeds = getCarriedSeedOptions(state);
        const carriedDeployables = getCarriedDeployableOptions(state);
        const carriedHammerCount = getCarriedItemQuantity(state, 14);
        const rootItems = [];
        const tileHasStructure = structureId !== 0;

        if (storageContainers.length === 1) {
            const containerInfo = storageContainers[0];
            rootItems.push({
                id: "open-storage:" + containerInfo.key,
                label: "Open Storage",
                meta: getContainerDisplayName(state, containerInfo),
                iconGlyph: "BX",
                iconLight: "#8f7a64",
                iconDark: "#5a4837",
                onSelect: function () {
                    openInventoryContainer(containerInfo);
                    closeTileContextMenu();
                }
            });
        } else if (storageContainers.length > 1) {
            rootItems.push({
                id: "open-storage",
                label: "Open Storage",
                meta: "Choose one attached container.",
                iconGlyph: "BX",
                iconLight: "#8f7a64",
                iconDark: "#5a4837",
                children: storageContainers.map((containerInfo) => ({
                    id: "open-storage:" + containerInfo.key,
                    label: getContainerDisplayName(state, containerInfo),
                    meta: "Open this container",
                    iconGlyph: "BX",
                    iconLight: "#8f7a64",
                    iconDark: "#5a4837",
                    onSelect: function () {
                        openInventoryContainer(containerInfo);
                        closeTileContextMenu();
                    }
                }))
            });
        } else if (tileHasStructure && structureProvidesStorage(structureId) && viewerCompatibilityWarning) {
            rootItems.push({
                id: "open-storage-unavailable",
                label: "Open Storage",
                meta: "Rebuild the visual smoke host and gameplay DLL.",
                iconGlyph: "BX",
                iconLight: "#8f7a64",
                iconDark: "#5a4837",
                disabled: true
            });
        }

        if (recipes.length > 0) {
            if (!craftContext) {
                rootItems.push({
                    id: "craft-loading",
                    label: "Craft",
                    meta: "Checking nearby supplies...",
                    iconGlyph: "CF",
                    iconLight: "#7a9d67",
                    iconDark: "#4a6b3b",
                    disabled: true
                });
            } else if (Array.isArray(craftContext.options) && craftContext.options.length > 0) {
            rootItems.push({
                id: "craft",
                label: "Craft",
                meta: "Choose one recipe.",
                iconGlyph: "CF",
                iconLight: "#7a9d67",
                iconDark: "#4a6b3b",
                    children: craftContext.options.map((option) => {
                    const itemMeta = getItemMeta(option.outputItemId);
                    const visual = getItemVisual(option.outputItemId);
                    return {
                        id: "craft:" + option.outputItemId,
                        label: itemMeta ? itemMeta.name : ("Item " + option.outputItemId),
                        meta: "Ready",
                        iconGlyph: visual.glyph,
                        iconLight: visual.light,
                        iconDark: visual.dark,
                        onSelect: function () {
                            statusChip.textContent =
                                "Moving to (" + tileX + "," + tileY + ") to craft " +
                                (itemMeta ? itemMeta.shortName : ("Item " + option.outputItemId)) + ".";
                            postSiteAction({
                                actionKind: "CRAFT",
                                flags: 4,
                                quantity: 1,
                                targetTileX: tileX,
                                targetTileY: tileY,
                                primarySubjectId: 0,
                                secondarySubjectId: 0,
                                itemId: option.outputItemId
                            }).catch(() => {
                                statusChip.textContent = "Failed to send craft action.";
                            });
                            closeTileContextMenu();
                        }
                    };
                })
            });
            }
        }

        if (tileHasStructure) {
            rootItems.push({
                id: "repair",
                label: "Repair",
                meta: carriedHammerCount > 0
                    ? ("Hammer x" + carriedHammerCount + " carried")
                    : "Requires a carried hammer.",
                iconGlyph: "HM",
                iconLight: "#9ea9b7",
                iconDark: "#5c6775",
                disabled: carriedHammerCount === 0,
                onSelect: function () {
                    statusChip.textContent =
                        "Moving to (" + tileX + "," + tileY + ") to repair " + getStructureLabel(structureId) + ".";
                    postSiteAction({
                        actionKind: "REPAIR",
                        flags: 4,
                        quantity: 1,
                        targetTileX: tileX,
                        targetTileY: tileY,
                        primarySubjectId: 0,
                        secondarySubjectId: 0,
                        itemId: 14
                    }).catch(() => {
                        statusChip.textContent = "Failed to send repair action.";
                    });
                    closeTileContextMenu();
                }
            });
        }

        if (!tileHasStructure && carriedSeeds.length > 0) {
            rootItems.push({
                id: "plant",
                label: "Plant",
                meta: "Choose one carried seed type.",
                iconGlyph: "PL",
                iconLight: "#7a9d67",
                iconDark: "#4a6b3b",
                children: carriedSeeds.map((seed) => {
                    return {
                        id: seed.id,
                        label: seed.label,
                        meta: "x" + seed.quantity + " carried",
                        iconGlyph: seed.iconGlyph,
                        iconLight: seed.iconLight,
                        iconDark: seed.iconDark,
                        onSelect: function () {
                            statusChip.textContent =
                                "Walking to tile (" + tileX + "," + tileY + ") to plant " + seed.shortLabel + ".";
                            postSiteAction({
                                actionKind: "PLANT",
                                flags: 4,
                                quantity: 1,
                                targetTileX: tileX,
                                targetTileY: tileY,
                                primarySubjectId: 0,
                                secondarySubjectId: 0,
                                itemId: seed.itemId
                            }).catch(() => {
                                statusChip.textContent = "Failed to send site action.";
                            });
                            closeTileContextMenu();
                        }
                    };
                })
            });
        }

        if (!tileHasStructure && carriedDeployables.length > 0) {
            rootItems.push({
                id: "deploy",
                label: "Deploy",
                meta: "Place a carried device kit here.",
                iconGlyph: "DP",
                iconLight: "#b48857",
                iconDark: "#6a4b2e",
                children: carriedDeployables.map((item) => {
                    return {
                        id: item.id,
                        label: item.label,
                        meta: "x" + item.quantity + " carried",
                        iconGlyph: item.iconGlyph,
                        iconLight: item.iconLight,
                        iconDark: item.iconDark,
                        onSelect: function () {
                            statusChip.textContent =
                                "Moving to (" + tileX + "," + tileY + ") to deploy " + item.shortLabel + ".";
                            postSiteAction({
                                actionKind: "BUILD",
                                flags: 4,
                                quantity: 1,
                                targetTileX: tileX,
                                targetTileY: tileY,
                                primarySubjectId: 0,
                                secondarySubjectId: 0,
                                itemId: item.itemId
                            }).catch(() => {
                                statusChip.textContent = "Failed to send build action.";
                            });
                            closeTileContextMenu();
                        }
                    };
                })
            });
        }

        if (rootItems.length === 0) {
            return [
                {
                    id: "none",
                    label: "No Action",
                    meta: tileHasStructure
                        ? "This structure has no direct action right now."
                        : "Carry seeds or deployable kits, or use a crafting/storage device.",
                    iconGlyph: "--",
                    iconLight: "#a38e76",
                    iconDark: "#796652",
                    disabled: true
                }
            ];
        }

        return rootItems;
    }

    function renderTileContextMenu() {
        if (!tileContextMenuState || !latestState || latestState.appState !== "SITE_ACTIVE") {
            closeTileContextMenu();
            return;
        }

        const rootItems = buildTileContextMenuItems(
            latestState,
            tileContextMenuState.tileX,
            tileContextMenuState.tileY);
        const panels = collectTileContextPanels(rootItems, tileContextMenuState.hoverPath);
        const panelWidth = 220;
        const panelGap = 10;
        const panelRise = 16;
        const viewportWidth = Math.max(window.innerWidth, 320);
        const viewportHeight = Math.max(window.innerHeight, 240);

        tileContextMenu.hidden = false;
        tileContextMenu.innerHTML = "";

        panels.forEach((panel) => {
            const panelElement = document.createElement("div");
            panelElement.className = "tile-context-panel";

            const panelLeft = Math.min(
                Math.max(10, tileContextMenuState.anchorX + panel.level * (panelWidth + panelGap)),
                Math.max(10, viewportWidth - panelWidth - 12));
            const estimatedHeight = panel.items.length * 58 + 18;
            const panelTop = Math.min(
                Math.max(10, tileContextMenuState.anchorY - panel.level * panelRise),
                Math.max(10, viewportHeight - estimatedHeight - 12));
            panelElement.style.left = panelLeft + "px";
            panelElement.style.top = panelTop + "px";

            panel.items.forEach((item) => {
                const button = document.createElement("button");
                button.type = "button";
                button.className = "tile-context-item";
                if (item.disabled) {
                    button.classList.add("disabled");
                }
                if (tileContextMenuState.hoverPath[panel.level] === item.id) {
                    button.classList.add("hovered");
                }
                button.disabled = !!item.disabled;
                button.addEventListener("mouseenter", function () {
                    setTileContextMenuHover(
                        panel.level,
                        item.id,
                        Array.isArray(item.children) && item.children.length > 0);
                });
                button.addEventListener("mousedown", function (event) {
                    event.stopPropagation();
                });
                button.addEventListener("click", function (event) {
                    event.preventDefault();
                    event.stopPropagation();
                    if (item.disabled) {
                        return;
                    }
                    if (typeof item.onSelect === "function") {
                        item.onSelect();
                        return;
                    }
                    setTileContextMenuHover(
                        panel.level,
                        item.id,
                        Array.isArray(item.children) && item.children.length > 0);
                });

                const icon = document.createElement("div");
                icon.className = "tile-context-item-icon";
                icon.textContent = item.iconGlyph || "--";
                if (item.iconLight) {
                    icon.style.setProperty("--menu-icon-light", item.iconLight);
                }
                if (item.iconDark) {
                    icon.style.setProperty("--menu-icon-dark", item.iconDark);
                }
                button.appendChild(icon);

                const main = document.createElement("div");
                main.className = "tile-context-item-main";
                const label = document.createElement("div");
                label.className = "tile-context-item-label";
                label.textContent = item.label;
                main.appendChild(label);
                if (item.meta) {
                    const meta = document.createElement("div");
                    meta.className = "tile-context-item-meta";
                    meta.textContent = item.meta;
                    main.appendChild(meta);
                }
                button.appendChild(main);

                const tail = document.createElement("div");
                tail.className = "tile-context-item-tail";
                tail.textContent = item.children && item.children.length > 0 ? ">" : "";
                button.appendChild(tail);

                panelElement.appendChild(button);
            });

            tileContextMenu.appendChild(panelElement);
        });
    }

    function openTileContextMenu(tileData, clientX, clientY) {
        if (!latestState || latestState.appState !== "SITE_ACTIVE") {
            return;
        }

        tileContextMenuState = {
            tileX: tileData && typeof tileData.tileX === "number" ? tileData.tileX : 0,
            tileY: tileData && typeof tileData.tileY === "number" ? tileData.tileY : 0,
            anchorX: clientX,
            anchorY: clientY,
            hoverPath: []
        };
        const tile = getTileSnapshot(latestState, tileContextMenuState.tileX, tileContextMenuState.tileY);
        const structureId = tile ? (tile.structureTypeId || 0) : 0;
        if (getCraftRecipesForStructure(structureId).length > 0) {
            postSiteContextRequest(tileContextMenuState.tileX, tileContextMenuState.tileY).catch(() => {
                statusChip.textContent = "Failed to query tile context.";
            });
        }
        renderTileContextMenu();
    }

    function hideInventoryTooltip() {
        inventoryTooltip.hidden = true;
    }

    function moveInventoryTooltip(clientX, clientY) {
        const tooltipWidth = 220;
        const tooltipHeight = 90;
        const left = Math.min(clientX + 18, Math.max(12, window.innerWidth - tooltipWidth - 16));
        const top = Math.min(clientY + 18, Math.max(12, window.innerHeight - tooltipHeight - 16));
        inventoryTooltip.style.left = left + "px";
        inventoryTooltip.style.top = top + "px";
    }

    function showInventoryTooltip(slot, options, clientX, clientY) {
        const occupied = isOccupiedSlot(slot);
        if (!occupied) {
            hideInventoryTooltip();
            return;
        }

        const itemMeta = getItemMeta(slot.itemId);
        const locationName = getContainerDisplayName(latestState, {
            containerKind: slot.containerKind,
            containerOwnerId: slot.containerOwnerId || 0,
            containerTileX: slot.containerTileX || 0,
            containerTileY: slot.containerTileY || 0
        });
        const actionHint = slot.containerKind === "WORKER_PACK"
            ? (itemMeta && itemMeta.canPlant ? "Click to select and arm/store" : "Click to select and use/store")
            : "Click to select and move";

        inventoryTooltipTitle.textContent = getInventoryItemLabel(slot);
        inventoryTooltipMeta.textContent =
            locationName + "  Slot " + (slot.slotIndex + 1) +
            "  x" + slot.quantity + "\n" +
            actionHint;
        inventoryTooltip.hidden = false;
        moveInventoryTooltip(clientX, clientY);
    }

    function clearSelectionInventory() {
        selectionInventory.hidden = true;
        selectionInventory.innerHTML = "";
        hideInventoryTooltip();
    }

    function clampMeterPercent(value) {
        return Math.round(clamp01((typeof value === "number" ? value : 0) / 100) * 100);
    }

    function appendInventorySummaryCard(container, label, valueText, fillPercent, fillClassName) {
        const card = document.createElement("div");
        card.className = "inventory-summary-card";

        const labelElement = document.createElement("div");
        labelElement.className = "inventory-summary-label";
        labelElement.textContent = label;
        card.appendChild(labelElement);

        const valueElement = document.createElement("div");
        valueElement.className = "inventory-summary-value" + (fillClassName === "money" ? " money" : "");
        valueElement.textContent = valueText;
        card.appendChild(valueElement);

        if (fillClassName !== "money") {
            const meterTrack = document.createElement("div");
            meterTrack.className = "inventory-meter-track";
            const meterFill = document.createElement("div");
            meterFill.className = "inventory-meter-fill " + fillClassName;
            meterFill.style.width = Math.max(0, Math.min(fillPercent, 100)) + "%";
            meterTrack.appendChild(meterFill);
            card.appendChild(meterTrack);
        }

        container.appendChild(card);
    }

    function appendInventorySummarySection(container, state) {
        const hud = getHudState(state);
        const siteState = getSiteState(state);
        const worker = siteState ? siteState.worker : null;
        const health = hud && typeof hud.playerHealth === "number"
            ? hud.playerHealth
            : (worker && typeof worker.healthNormalized === "number" ? worker.healthNormalized * 100 : 0);
        const hydration = hud && typeof hud.playerHydration === "number"
            ? hud.playerHydration
            : (worker && typeof worker.hydrationNormalized === "number" ? worker.hydrationNormalized * 100 : 0);
        const energy = hud && typeof hud.playerEnergy === "number"
            ? hud.playerEnergy
            : (worker && typeof worker.energyNormalized === "number" ? worker.energyNormalized * 100 : 0);
        const currentMoney = hud && typeof hud.currentMoney === "number" ? hud.currentMoney : 0;

        const section = document.createElement("section");
        section.className = "inventory-section";

        const header = document.createElement("div");
        header.className = "inventory-section-header";

        const titleElement = document.createElement("div");
        titleElement.className = "inventory-section-title";
        titleElement.textContent = "Field Status";
        header.appendChild(titleElement);

        const metaElement = document.createElement("div");
        metaElement.className = "inventory-section-meta";
        metaElement.textContent = "HUD-driven values  |  Toggle B";
        header.appendChild(metaElement);
        section.appendChild(header);

        const summaryGrid = document.createElement("div");
        summaryGrid.className = "inventory-summary-grid";
        section.appendChild(summaryGrid);

        appendInventorySummaryCard(summaryGrid, "Health", clampMeterPercent(health) + "%", clampMeterPercent(health), "health");
        appendInventorySummaryCard(summaryGrid, "Hydration", clampMeterPercent(hydration) + "%", clampMeterPercent(hydration), "hydration");
        appendInventorySummaryCard(summaryGrid, "Energy", clampMeterPercent(energy) + "%", clampMeterPercent(energy), "energy");
        appendInventorySummaryCard(summaryGrid, "Money", "$" + String(currentMoney), 0, "money");

        container.appendChild(section);
    }

    function makeInventorySection(title, metaText, gridClassName, columnCount) {
        const section = document.createElement("section");
        section.className = "inventory-section";

        const header = document.createElement("div");
        header.className = "inventory-section-header";

        const titleElement = document.createElement("div");
        titleElement.className = "inventory-section-title";
        titleElement.textContent = title;
        header.appendChild(titleElement);

        const metaElement = document.createElement("div");
        metaElement.className = "inventory-section-meta";
        metaElement.textContent = metaText;
        header.appendChild(metaElement);
        section.appendChild(header);

        const grid = document.createElement("div");
        grid.className = "inventory-grid" + (gridClassName ? " " + gridClassName : "");
        grid.style.setProperty("--inventory-columns", String(columnCount));
        section.appendChild(grid);

        return { section, grid };
    }

    function appendSlotAction(container, label, onClick, variant, disabled) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "slot-action-button" + (variant ? " " + variant : "");
        button.textContent = label;
        button.disabled = !!disabled;
        button.addEventListener("click", function (event) {
            event.preventDefault();
            event.stopPropagation();
            if (!button.disabled) {
                onClick();
            }
        });
        container.appendChild(button);
    }

    function isOccupiedSlot(slot) {
        return !!slot && slot.flags !== 0 && slot.itemId !== 0 && slot.quantity > 0;
    }

    function handleInventorySlotPrimaryPress(slot, event) {
        if (event.button !== 0) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();

        const primaryTransferClick = slotUsesPrimaryTransferClick(latestState, slot);
        const transferTargetContainer = findPrimaryTransferTargetContainer(latestState, slot);

        if (transferTargetContainer) {
            selectedInventorySlotKey = "";
            postInventoryTransfer(
                slot,
                transferTargetContainer.containerKind,
                transferTargetContainer.containerOwnerId).catch(() => {
                statusChip.textContent = "Failed to move inventory item.";
            });
            if (latestState) {
                renderSiteOverlay(latestState);
            }
            return;
        }

        if (primaryTransferClick) {
            return;
        }

        if (!slotSupportsSelection(slot)) {
            return;
        }

        const clickedKey = slotKey(slot.containerKind, slot.slotIndex, slot.containerOwnerId);
        selectedInventorySlotKey = selectedInventorySlotKey === clickedKey ? "" : clickedKey;
        if (latestState) {
            renderSiteOverlay(latestState);
        }
    }

    function createInventorySlotCard(label, slot, options) {
        const itemMeta = slot ? getItemMeta(slot.itemId) : null;
        const occupied = isOccupiedSlot(slot);
        const card = document.createElement("div");
        const slotClasses = ["inventory-slot"];
        if (!occupied) {
            slotClasses.push("empty");
        }
        if (occupied && itemMeta && itemMeta.canPlant) {
            slotClasses.push("plantable");
        }
        if (occupied && itemMeta && itemMeta.canUse) {
            slotClasses.push("usable");
        }
        if (options.slotClassName) {
            slotClasses.push(options.slotClassName);
        }
        if (occupied && options.armedSlotKey && slotKey(slot.containerKind, slot.slotIndex, slot.containerOwnerId) === options.armedSlotKey) {
            slotClasses.push("armed");
        }
        if (occupied && options.selectedSlotKey && slotKey(slot.containerKind, slot.slotIndex, slot.containerOwnerId) === options.selectedSlotKey) {
            slotClasses.push("selected");
        }
        card.className = slotClasses.join(" ");
        card.title = occupied ? getInventoryItemLabel(slot) : label;

        const icon = document.createElement("div");
        icon.className = "inventory-slot-icon";
        if (occupied) {
            const visual = getItemVisual(slot.itemId);
            icon.style.setProperty("--slot-accent-light", visual.light);
            icon.style.setProperty("--slot-accent-dark", visual.dark);
            const glyph = document.createElement("div");
            glyph.className = "inventory-slot-icon-glyph";
            glyph.textContent = visual.glyph;
            icon.appendChild(glyph);
        } else {
            const glyph = document.createElement("div");
            glyph.className = "inventory-slot-icon-glyph";
            glyph.textContent = "--";
            icon.appendChild(glyph);
        }
        card.appendChild(icon);

        const corner = document.createElement("div");
        corner.className = "inventory-slot-corner";
        card.appendChild(corner);

        if (occupied) {
            const count = document.createElement("div");
            count.className = "inventory-slot-count";
            count.textContent = String(slot.quantity);
            card.appendChild(count);
        }

        if (occupied && options.interactive !== false) {
            card.addEventListener("mouseenter", function (event) {
                showInventoryTooltip(slot, options, event.clientX, event.clientY);
            });
            card.addEventListener("mousemove", function (event) {
                showInventoryTooltip(slot, options, event.clientX, event.clientY);
            });
            card.addEventListener("mouseleave", function () {
                hideInventoryTooltip();
            });
        }

        if (occupied && options.interactive !== false) {
            card.addEventListener("mousedown", function (event) {
                handleInventorySlotPrimaryPress(slot, event);
            });
        }

        return card;
    }

    function appendInventoryGridSection(container, title, metaText, slots, options) {
        const sectionParts = makeInventorySection(title, metaText, options.gridClassName || "", options.columns);
        const slotByIndex = new Map();
        slots.forEach((slot) => {
            slotByIndex.set(slot.slotIndex, slot);
        });

        for (let slotIndex = 0; slotIndex < options.slotCount; slotIndex += 1) {
            const slot = slotByIndex.get(slotIndex) || {
                containerKind: options.containerKind,
                containerOwnerId: options.containerOwnerId || 0,
                containerTileX: options.containerTileX || 0,
                containerTileY: options.containerTileY || 0,
                slotIndex: slotIndex,
                itemId: 0,
                quantity: 0,
                flags: 0,
                condition: 0,
                freshness: 0
            };
            sectionParts.grid.appendChild(createInventorySlotCard(options.slotLabelPrefix + " " + (slotIndex + 1), slot, options));
        }

        container.appendChild(sectionParts.section);
    }

    function parseRegionalLoadoutSlots(labels) {
        return labels
            .map((entry) => {
                if (!entry || typeof entry.text !== "string") {
                    return null;
                }

                const match = /^(.*)\sx(\d+)$/.exec(entry.text.trim());
                if (!match) {
                    return null;
                }

                const itemName = match[1].trim();
                const quantity = Number(match[2]) || 0;
                const itemId = findItemIdByLabel(itemName);
                return {
                    containerKind: "LOADOUT",
                    containerOwnerId: 0,
                    containerTileX: 0,
                    containerTileY: 0,
                    slotIndex: 0,
                    itemId: itemId,
                    itemName: itemName,
                    quantity: quantity,
                    flags: quantity > 0 ? 1 : 0,
                    condition: 1,
                    freshness: 1
                };
            })
            .filter((slot) => !!slot)
            .map((slot, index) => ({
                ...slot,
                slotIndex: index
            }));
    }

    function renderRegionalMapLoadoutPanel(state, selectionSetup, primaryLabelText) {
        const labels = getLabelElements(selectionSetup);
        const loadoutSlots = parseRegionalLoadoutSlots(labels);

        selectionInventory.innerHTML = "";
        if (state.selectedSiteId == null || loadoutSlots.length === 0) {
            selectionInventory.hidden = true;
            return;
        }

        selectionInventory.hidden = false;
        const stack = document.createElement("div");
        stack.className = "site-panel-stack";
        selectionInventory.appendChild(stack);

        appendInventoryGridSection(
            stack,
            "Deployment Loadout",
            primaryLabelText ? (primaryLabelText + "  |  Starter support ready") : "Starter support ready",
            loadoutSlots,
            {
                state: state,
                containerKind: "LOADOUT",
                slotCount: loadoutSlots.length,
                columns: Math.min(4, Math.max(loadoutSlots.length, 1)),
                gridClassName: "loadout-grid",
                slotClassName: "loadout-slot",
                slotLabelPrefix: "Loadout",
                selectedSlotKey: "",
                interactive: false
            });

        const footnote = document.createElement("div");
        footnote.className = "inventory-footnote";
        footnote.textContent =
            "Baseline support is packed before deployment. Water and recovery supplies start carried; seeds and raw materials begin in the starter storage crate.";
        stack.appendChild(footnote);
    }

    function renderSiteInventoryPanel(state, workerPackSlots) {
        selectionInventory.hidden = false;
        selectionInventory.innerHTML = "";

        if (!inventoryPanelOpen) {
            const hiddenNote = document.createElement("div");
            hiddenNote.className = "inventory-panel-hidden-note";
            hiddenNote.textContent = "Inventory panel hidden. Press B to reopen.";
            selectionInventory.appendChild(hiddenNote);
            hideInventoryTooltip();
            return;
        }

        const stack = document.createElement("div");
        stack.className = "site-panel-stack";
        selectionInventory.appendChild(stack);

        appendInventorySummarySection(stack, state);

        const topGrid = document.createElement("div");
        topGrid.className = "site-panel-grid";
        stack.appendChild(topGrid);

        appendInventoryGridSection(topGrid, "Worker Pack", "Live carried inventory", workerPackSlots, {
            state: state,
            containerKind: "WORKER_PACK",
            slotCount: 6,
            columns: 3,
            slotLabelPrefix: "Pack",
            selectedSlotKey: selectedInventorySlotKey
        });

        const footnote = document.createElement("div");
        footnote.className = "inventory-footnote";
        const carriedSeeds = getCarriedSeedOptions(state);
        const carriedDeployables = getCarriedDeployableOptions(state);
        const footnoteParts = [];
        footnoteParts.push("Press B to toggle the worker pack. Right-click a storage or crafting device to open its separate storage panel, then left-click items to move them between that storage and the pack.");
        if (carriedSeeds.length > 0) {
            footnoteParts.push(
                "Seeds: " +
                carriedSeeds.map((seed) => seed.shortLabel + " x" + seed.quantity).join("  | "));
        }
        if (carriedDeployables.length > 0) {
            footnoteParts.push(
                "Deployables: " +
                carriedDeployables.map((item) => item.shortLabel + " x" + item.quantity).join("  | "));
        }
        footnote.textContent = footnoteParts.join("     ");
        stack.appendChild(footnote);
    }

    function renderStoragePanel(state, openedContainerInfo) {
        const currentOpenedContainer =
            state && state.appState === "SITE_ACTIVE"
                ? findOpenedInventoryContainer(state)
                : null;
        if (!state || state.appState !== "SITE_ACTIVE" || !currentOpenedContainer) {
            storagePanel.hidden = true;
            storagePanelBody.innerHTML = "";
            return;
        }

        storagePanel.hidden = false;
        storagePanelTitle.textContent = getContainerDisplayName(state, currentOpenedContainer);

        const tile = getTileSnapshot(state, currentOpenedContainer.containerTileX, currentOpenedContainer.containerTileY);
        const structureMeta = getStructureMeta(tile ? tile.structureTypeId : 0);
        const slotCount = Math.max(
            currentOpenedContainer.slotCount || 0,
            structureMeta ? structureMeta.slotCount : 0,
            currentOpenedContainer.slots.length,
            1);
        storagePanelSubtitle.textContent = currentOpenedContainer.isOpenedView
            ? (structureMeta
                ? ("Device storage - " + slotCount + " slots")
                : "Device storage")
            : "Loading storage snapshot...";

        storagePanelBody.innerHTML = "";
        const stack = document.createElement("div");
        stack.className = "site-panel-stack";
        storagePanelBody.appendChild(stack);

        appendInventoryGridSection(
            stack,
            storagePanelTitle.textContent,
            storagePanelSubtitle.textContent,
            currentOpenedContainer.slots,
            {
                state: state,
                containerKind: currentOpenedContainer.containerKind,
                containerOwnerId: currentOpenedContainer.containerOwnerId,
                containerTileX: currentOpenedContainer.containerTileX,
                containerTileY: currentOpenedContainer.containerTileY,
                slotCount: slotCount,
                columns: slotCount <= 6 ? 3 : 4,
                slotLabelPrefix: "Device",
                selectedSlotKey: selectedInventorySlotKey
            });
    }

    function appendSelectedInventoryActions(state, selectedSlot) {
        if (!selectedSlot) {
            const helper = document.createElement("div");
            helper.className = "helper-note";
            helper.textContent =
                "Hover any item for details. Click pack items to move them into the opened storage, or click opened storage items to carry them into the pack. Usable carried items still surface actions here when no storage is open.";
            contextActions.appendChild(helper);
            return;
        }

        const itemMeta = getItemMeta(selectedSlot.itemId);
        const slotSummary = document.createElement("div");
        slotSummary.className = "helper-note";
        slotSummary.textContent =
            getInventoryItemLabel(selectedSlot) +
            "  x" + selectedSlot.quantity +
            "  " +
            getContainerDisplayName(state, {
                containerKind: selectedSlot.containerKind,
                containerOwnerId: selectedSlot.containerOwnerId || 0,
                containerTileX: selectedSlot.containerTileX || 0,
                containerTileY: selectedSlot.containerTileY || 0
            }) +
            " slot " + (selectedSlot.slotIndex + 1);
        contextActions.appendChild(slotSummary);

        if (selectedSlot.containerKind === "WORKER_PACK" && itemMeta && itemMeta.canUse) {
            contextActions.appendChild(
                makeButton(
                    (itemMeta.useLabel || "Use") + " " + getInventoryItemLabel(selectedSlot),
                    function () {
                        postInventoryUse(selectedSlot).catch(() => {
                            statusChip.textContent = "Failed to use inventory item.";
                        });
                    },
                    false,
                    false
                )
            );
        }

        contextActions.appendChild(
            makeButton(
                "Clear Selection",
                function () {
                    selectedInventorySlotKey = "";
                    if (latestState) {
                        renderSiteOverlay(latestState);
                    }
                },
                true,
                false
            )
        );
    }

    function clearPhonePanel() {
        phoneScreenBody.innerHTML = "";
        phoneLayer.classList.remove("open");
        phoneLayer.classList.add("hidden-view");
        phoneLayer.setAttribute("aria-hidden", "true");
        stageFrame.classList.remove("phone-open");
    }

    function syncPhonePanelVisibility(isSiteActive) {
        if (!isSiteActive) {
            clearPhonePanel();
            return;
        }

        phoneLayer.classList.remove("hidden-view");
        phoneLayer.classList.toggle("open", phonePanelOpen);
        phoneLayer.setAttribute("aria-hidden", phonePanelOpen ? "false" : "true");
        stageFrame.classList.toggle("phone-open", phonePanelOpen);
    }

    function formatPhoneClockLabel() {
        const now = new Date();
        const hours = String(now.getHours()).padStart(2, "0");
        const minutes = String(now.getMinutes()).padStart(2, "0");
        return hours + ":" + minutes;
    }

    function countTasksByListKind(tasks, listKind) {
        return tasks.filter((task) => task.listKind === listKind).length;
    }

    function appendPhoneSummaryCard(container, label, value) {
        const card = document.createElement("div");
        card.className = "phone-summary-card";

        const labelElement = document.createElement("div");
        labelElement.className = "phone-summary-label";
        labelElement.textContent = label;
        card.appendChild(labelElement);

        const valueElement = document.createElement("div");
        valueElement.className = "phone-summary-value";
        valueElement.textContent = value;
        card.appendChild(valueElement);

        container.appendChild(card);
    }

    function makePhoneSection(title, metaText) {
        const section = document.createElement("section");
        section.className = "phone-section";

        const header = document.createElement("div");
        header.className = "phone-section-header";

        const titleElement = document.createElement("div");
        titleElement.className = "phone-section-title";
        titleElement.textContent = title;
        header.appendChild(titleElement);

        const metaElement = document.createElement("div");
        metaElement.className = "phone-section-meta";
        metaElement.textContent = metaText;
        header.appendChild(metaElement);

        section.appendChild(header);
        return section;
    }

    function getPhoneListingBadgeLabel(listing) {
        switch (listing.listingKind) {
        case "SELL_ITEM":
            return "Sell";
        case "HIRE_CONTRACTOR":
            return "Hire";
        case "PURCHASE_UNLOCKABLE":
            return "Unlock";
        case "BUY_ITEM":
        default:
            return "Buy";
        }
    }

    function getPhoneListingTitle(listing) {
        switch (listing.listingKind) {
        case "SELL_ITEM":
        case "BUY_ITEM":
            return getItemLabel(listing.itemOrUnlockableId);
        case "HIRE_CONTRACTOR":
            return "Hire Contractor";
        case "PURCHASE_UNLOCKABLE":
            return "Unlockable " + listing.itemOrUnlockableId;
        default:
            return "Listing " + listing.listingId;
        }
    }

    function getPhoneListingMetaText(listing, affordable) {
        const parts = ["$" + listing.price];
        if (listing.listingKind === "SELL_ITEM") {
            parts.push("available x" + listing.quantity);
        } else if (listing.listingKind === "HIRE_CONTRACTOR") {
            parts.push("open slots x" + listing.quantity);
        } else {
            parts.push("stock x" + listing.quantity);
        }
        if (listing.relatedSiteId) {
            parts.push("site " + listing.relatedSiteId);
        }
        if (!affordable && listing.listingKind !== "SELL_ITEM") {
            parts.push("insufficient funds");
        }
        return parts.join("  |  ");
    }

    function getPhoneListingActionLabel(listing) {
        switch (listing.listingKind) {
        case "SELL_ITEM":
            return "Sell For $" + listing.price;
        case "HIRE_CONTRACTOR":
            return "Hire For $" + listing.price;
        case "PURCHASE_UNLOCKABLE":
            return "Unlock For $" + listing.price;
        case "BUY_ITEM":
        default:
            return "Buy For $" + listing.price;
        }
    }

    function canUsePhoneListing(state, listing) {
        if (listing.listingKind === "SELL_ITEM") {
            return listing.quantity > 0;
        }

        const hud = getHudState(state);
        const currentMoney = hud && typeof hud.currentMoney === "number" ? hud.currentMoney : 0;
        return listing.quantity > 0 && currentMoney >= listing.price;
    }

    function postPhoneListingAction(listing) {
        return postJson("/ui-action", {
            type: listing.listingKind === "SELL_ITEM" ? "SELL_PHONE_LISTING" : "BUY_PHONE_LISTING",
            targetId: listing.listingId,
            arg0: 1,
            arg1: 0
        });
    }

    function appendPhoneListingSection(container, title, metaText, listings, state) {
        if (listings.length === 0) {
            return;
        }

        const section = makePhoneSection(title, metaText);
        const stack = document.createElement("div");
        stack.className = "phone-list-stack";
        section.appendChild(stack);

        listings.forEach((listing) => {
            const affordable = canUsePhoneListing(state, listing);
            const card = document.createElement("article");
            card.className = "phone-list-card" + (affordable ? "" : " disabled");

            const header = document.createElement("div");
            header.className = "phone-list-header";

            const titleElement = document.createElement("div");
            titleElement.className = "phone-list-title";
            titleElement.textContent = getPhoneListingTitle(listing);
            header.appendChild(titleElement);

            const badge = document.createElement("div");
            badge.className = "phone-list-badge";
            badge.textContent = getPhoneListingBadgeLabel(listing);
            header.appendChild(badge);
            card.appendChild(header);

            const metaElement = document.createElement("div");
            metaElement.className = "phone-list-meta";
            metaElement.textContent = getPhoneListingMetaText(listing, affordable);
            card.appendChild(metaElement);

            card.appendChild(
                makePhoneActionButton(
                    getPhoneListingActionLabel(listing),
                    function () {
                        postPhoneListingAction(listing).catch(() => {
                            statusChip.textContent = listing.listingKind === "SELL_ITEM"
                                ? "Failed to sell listing."
                                : "Failed to use phone listing.";
                        });
                    },
                    listing.listingKind === "SELL_ITEM",
                    !affordable
                )
            );

            stack.appendChild(card);
        });

        container.appendChild(section);
    }

    function getPhoneTaskStateLabel(task) {
        switch (task.listKind) {
        case "ACCEPTED":
            return "Active";
        case "COMPLETED":
            return "Done";
        case "VISIBLE":
        default:
            return "Open";
        }
    }

    function getPhoneTaskStateClassName(task) {
        let className = "phone-task-state";
        if (task.listKind === "ACCEPTED") {
            className += " accepted";
        } else if (task.listKind === "COMPLETED") {
            className += " completed";
        }
        return className;
    }

    function appendPhoneTaskSection(container, tasks) {
        const visibleTaskCount = countTasksByListKind(tasks, "VISIBLE");
        const acceptedTaskCount = countTasksByListKind(tasks, "ACCEPTED");
        const section = makePhoneSection("Contracts", "Open " + visibleTaskCount + "  |  Active " + acceptedTaskCount);

        if (tasks.length === 0) {
            const emptyState = document.createElement("div");
            emptyState.className = "phone-empty-state";
            emptyState.textContent = "No field contracts are synced to the phone yet.";
            section.appendChild(emptyState);
            container.appendChild(section);
            return;
        }

        const stack = document.createElement("div");
        stack.className = "phone-list-stack";
        section.appendChild(stack);

        tasks.forEach((task) => {
            const targetProgress = Math.max(task.targetProgress || 0, 1);
            const currentProgress = Math.max(0, Math.min(task.currentProgress || 0, targetProgress));
            const completion = Math.round((currentProgress / targetProgress) * 100);
            const card = document.createElement("article");
            card.className = "phone-task-card";

            const header = document.createElement("div");
            header.className = "phone-task-header";

            const titleElement = document.createElement("div");
            titleElement.className = "phone-task-title";
            titleElement.textContent = "Contract " + task.taskInstanceId;
            header.appendChild(titleElement);

            const stateElement = document.createElement("div");
            stateElement.className = getPhoneTaskStateClassName(task);
            stateElement.textContent = getPhoneTaskStateLabel(task);
            header.appendChild(stateElement);
            card.appendChild(header);

            const metaElement = document.createElement("div");
            metaElement.className = "phone-task-meta";
            metaElement.textContent =
                "Template " + task.taskTemplateId +
                "  |  Progress " + currentProgress + "/" + targetProgress +
                "  |  " + completion + "%";
            card.appendChild(metaElement);

            const track = document.createElement("div");
            track.className = "phone-task-track";
            const fill = document.createElement("div");
            fill.className = "phone-task-fill";
            fill.style.width = completion + "%";
            track.appendChild(fill);
            card.appendChild(track);

            stack.appendChild(card);
        });

        container.appendChild(section);
    }

    function renderPhonePanel(state) {
        const isSiteActive = !!state && state.appState === "SITE_ACTIVE";
        syncPhonePanelVisibility(isSiteActive);
        phoneScreenBody.innerHTML = "";

        if (!isSiteActive || !phonePanelOpen) {
            return;
        }

        phoneStatusTime.textContent = formatPhoneClockLabel();

        const hud = getHudState(state);
        const tasks = getSiteTasks(state);
        const buyListings = getBuyListings(state);
        const sellListings = getSellListings(state);
        const specialListings = getSpecialPhoneListings(state);
        const currentMoney = hud && typeof hud.currentMoney === "number" ? hud.currentMoney : 0;
        const activeTaskCount = hud && typeof hud.activeTaskCount === "number"
            ? hud.activeTaskCount
            : countTasksByListKind(tasks, "ACCEPTED");
        const visibleTaskCount = countTasksByListKind(tasks, "VISIBLE");

        phoneAppSubtitle.textContent =
            activeTaskCount > 0
                ? activeTaskCount + " active contract" + (activeTaskCount === 1 ? "" : "s") + ". Press F to pocket the handset."
                : "Pocket market access, contracts, and remote services. Press F to pocket it again.";

        const summaryGrid = document.createElement("div");
        summaryGrid.className = "phone-summary-grid";
        appendPhoneSummaryCard(summaryGrid, "Money", "$" + String(currentMoney));
        appendPhoneSummaryCard(summaryGrid, "Buy", String(buyListings.length));
        appendPhoneSummaryCard(summaryGrid, "Sell", String(sellListings.length));
        appendPhoneSummaryCard(summaryGrid, "Tasks", String(activeTaskCount + visibleTaskCount));
        phoneScreenBody.appendChild(summaryGrid);

        appendPhoneTaskSection(phoneScreenBody, tasks);
        appendPhoneListingSection(phoneScreenBody, "Marketplace", "Remote orders routed to camp stock", buyListings, state);
        appendPhoneListingSection(phoneScreenBody, "Sellback", "Current inventory surfaced for quick sale", sellListings, state);
        appendPhoneListingSection(phoneScreenBody, "Services", "Remote services and unlockables", specialListings, state);

        if (buyListings.length === 0 && sellListings.length === 0 && specialListings.length === 0) {
            const emptyState = document.createElement("div");
            emptyState.className = "phone-empty-state";
            emptyState.textContent = "No remote listings are available on the phone right now.";
            phoneScreenBody.appendChild(emptyState);
        }
    }

    function updateMoveAxis() {
        const x = (keys.has("KeyD") || keys.has("ArrowRight") ? 1 : 0) - (keys.has("KeyA") || keys.has("ArrowLeft") ? 1 : 0);
        const y = (keys.has("KeyW") || keys.has("ArrowUp") ? 1 : 0) - (keys.has("KeyS") || keys.has("ArrowDown") ? 1 : 0);
        moveAxes.x = x;
        moveAxes.y = y;
    }

    function renderMenuOverlay(state) {
        const menuSetup = getSetup(state, "MAIN_MENU");
        const actionElements = getActionElements(menuSetup);

        menuPanel.hidden = false;
        menuEyebrow.textContent = "Campaign 01";
        menuTitle.textContent = "GS1";
        menuSubtitle.textContent = "Desert Restoration Survival";
        menuCopy.textContent = "The frontier is failing under heat, wind, and moving sand. Establish shelter, hold a line of living cover, and push the campaign site by site before exposed ground takes the region back.";
        menuMeta.innerHTML =
            "<span class=\"menu-meta-chip\">4-Site Prototype</span>" +
            "<span class=\"menu-meta-chip life\">Living Cover</span>" +
            "<span class=\"menu-meta-chip water\">Water Discipline</span>";
        menuActions.innerHTML = "";

        actionElements.forEach((element, index) => {
            const label =
                element.action && element.action.type === "START_NEW_CAMPAIGN"
                    ? "Begin Campaign"
                    : (element.text || element.action.type);
            menuActions.appendChild(
                makeButton(
                    label,
                    function () {
                        postJson("/ui-action", element.action).catch(() => {
                            statusChip.textContent = "Failed to send UI action.";
                        });
                    },
                    index > 0,
                    (element.flags & 2) !== 0
                )
            );

            const appendedButton = menuActions.lastElementChild;
            if (appendedButton && element.action && element.action.type === "START_NEW_CAMPAIGN") {
                appendedButton.classList.add("title-primary-action");
            }
        });

        selectionEyebrow.textContent = "Situation";
        selectionText.textContent = "Supplies are thin, cover is fragile, and every deployment begins exposed to dust and heat. Begin the campaign and secure the first living foothold.";
        renderStoragePanel(null, null);
        contextActions.innerHTML = "";
    }

    function renderRegionalMapOverlay(state) {
        const selectionSetup = getSetup(state, "REGIONAL_MAP_SELECTION");
        const labels = getLabelElements(selectionSetup);
        const actions = getVisibleActionElements(selectionSetup);
        const primaryLabel = labels.length > 0 ? labels[0].text : "";

        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Field Survey";

        if (primaryLabel) {
            selectionText.textContent = primaryLabel;
        } else if (state.selectedSiteId != null) {
            selectionText.textContent = "Deployment route prepared for Site " + state.selectedSiteId + ". Review the dossier and commit when ready.";
        } else {
            selectionText.textContent = "Review the campaign survey and choose the next exposed site to stabilize.";
        }

        renderRegionalMapLoadoutPanel(state, selectionSetup, primaryLabel);
        renderStoragePanel(null, null);

        contextActions.innerHTML = "";
        actions.forEach((element) => {
            let label = element.text || element.action.type;
            if (element.action && element.action.type === "START_SITE_ATTEMPT") {
                label = "Deploy To Site " + element.action.targetId;
            } else if (element.action && element.action.type === "CLEAR_DEPLOYMENT_SITE_SELECTION") {
                label = "Clear Route";
            }

            contextActions.appendChild(
                makeButton(
                    label,
                    function () {
                        postJson("/ui-action", element.action).catch(() => {
                            statusChip.textContent = "Failed to send UI action.";
                        });
                    },
                    false,
                    (element.flags & 2) !== 0
                )
            );
        });
    }

    function renderFallbackOverlay(state) {
        const siteBootstrap = getSiteBootstrap(state);
        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Current View";
        selectionText.textContent =
            siteBootstrap
                ? "Site " + siteBootstrap.siteId + " placeholder view."
                : "Waiting for the next presentation state.";
        selectionInventory.hidden = true;
        selectionInventory.innerHTML = "";
        renderStoragePanel(null, null);
        contextActions.innerHTML = "";
    }

    function renderSiteOverlay(state) {
        const siteBootstrap = getSiteBootstrap(state);
        const workerPackSlots = getInventorySlotsByKind(state, "WORKER_PACK");
        const carriedSeeds = getCarriedSeedOptions(state);
        const carriedDeployables = getCarriedDeployableOptions(state);

        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Site Active";
        contextActions.innerHTML = "";
        clearSelectedInventorySlotIfInvalid(state);
        clearOpenedInventoryContainerIfInvalid(state);

        const selectedSlot = findSelectedInventorySlot(state);
        const openedContainerInfo = findOpenedInventoryContainer(state);
        const plantingText = carriedSeeds.length > 0
            ? ("Right-click open ground to plant. Carried seeds: " +
                carriedSeeds.map((seed) => seed.shortLabel + " x" + seed.quantity).join("  |  "))
            : "Carry seeds into the worker pack to unlock planting from the tile menu.";
        const deployText = carriedDeployables.length > 0
            ? ("Deployable kits: " +
                carriedDeployables.map((item) => item.shortLabel + " x" + item.quantity).join("  | "))
            : "Craft or collect deployable kits, then right-click open ground to place them.";
        const storageText = openedContainerInfo
            ? ("Opened container: " + getContainerDisplayName(state, openedContainerInfo) + ". Left-click pack items to send them there, or left-click storage items to carry them back into the pack.")
            : "Right-click a storage crate, workbench, stove, or another storage device to open that specific container. Items move between storage and the worker pack one click at a time.";
        selectionText.innerHTML = siteBootstrap
            ? (
                "Site " + siteBootstrap.siteId +
                " is live. Use WASD to move, press B to open or close the worker pack, press F to open or close the phone, drag with right mouse to orbit the camera, and short right-click a tile to choose a site action." +
                "<br><br>" + plantingText +
                "<br><br>" + deployText +
                "<br><br>" + storageText +
                "<br><br>Inventory and storage panels resolve slots from the adapter's cached site snapshot and refresh as inventory slot updates arrive."
            )
            : "Site bootstrap is loading.";
        renderSiteInventoryPanel(state, workerPackSlots);
        renderStoragePanel(state, openedContainerInfo);
        appendSelectedInventoryActions(state, selectedSlot);
        renderSiteStatusChip(state);
    }

    function updateOverlay(state) {
        stageFrame.classList.toggle("main-menu-mode", state.appState === "MAIN_MENU");
        stageFrame.classList.toggle("regional-map-mode", state.appState === "REGIONAL_MAP");
        stageFrame.classList.toggle("site-active-mode", state.appState === "SITE_ACTIVE");
        hudEyebrow.textContent = "App State";
        hudTitle.textContent = state.appState || "NONE";
        const appStateChanged = lastOverlayAppState !== state.appState;

        switch (state.appState) {
        case "MAIN_MENU":
            hudSubtitle.textContent = "Austere, painterly, and severe: the campaign opens under hostile conditions.";
            inventoryPanelOpen = true;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            clearSelectionInventory();
            renderMenuOverlay(state);
            statusChip.textContent = "Prototype Build\nVisual Smoke";
            break;
        case "REGIONAL_MAP":
            hudSubtitle.textContent = "Review the campaign survey board and choose the next deployment route.";
            inventoryPanelOpen = true;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            clearSelectionInventory();
            renderRegionalMapOverlay(state);
            statusChip.textContent =
                "Campaign Survey\nFrame " + state.frameNumber +
                "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            break;
        case "SITE_ACTIVE":
            if (appStateChanged) {
                inventoryPanelOpen = false;
                phonePanelOpen = false;
                selectedInventorySlotKey = "";
                openedInventoryContainerKey = "";
            }
            hudSubtitle.textContent =
                "Field movement is now live. Press B to toggle the inventory panel, press F to pocket or raise the phone, drag with right mouse to orbit the follow camera, or short right-click a tile to open its action menu.";
            renderSiteOverlay(state);
            break;
        default:
            hudSubtitle.textContent = "The current adapter only styles the core early flow for now.";
            inventoryPanelOpen = true;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            clearSelectionInventory();
            renderFallbackOverlay(state);
            statusChip.textContent =
                "Connected\nFrame " + state.frameNumber +
                "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            break;
        }

        renderPhonePanel(state);
        lastOverlayAppState = state.appState;
    }

    function renderMainMenuScene() {
        currentSceneKind = "MAIN_MENU";
        scene.background = new THREE_NS.Color(0xebdcc2);

        const floor = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(8.5, 10.5, 0.5, 48),
            new THREE_NS.MeshStandardMaterial({ color: 0xd6b587, roughness: 0.92, metalness: 0.02 })
        );
        floor.position.set(0, -0.25, 0);
        worldGroup.add(floor);

        const tower = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(1.6, 4.4, 1.6),
            new THREE_NS.MeshStandardMaterial({ color: 0x8d6237, roughness: 0.75, metalness: 0.04 })
        );
        tower.position.set(0, 2.0, 0);
        tower.userData = { spinY: 0.06 };
        worldGroup.add(tower);

        const halo = new THREE_NS.Mesh(
            new THREE_NS.TorusGeometry(3.0, 0.11, 16, 64),
            new THREE_NS.MeshStandardMaterial({ color: 0xf4f0e6, emissive: 0xcfa76d, emissiveIntensity: 0.35 })
        );
        halo.rotation.x = Math.PI / 2.3;
        halo.position.set(0, 2.45, 0);
        halo.userData = { spinZ: 0.3, bobAmplitude: 0.12, bobOffset: 0.3, baseY: 2.45 };
        worldGroup.add(halo);

        const orbit = new THREE_NS.Mesh(
            new THREE_NS.TorusGeometry(4.2, 0.07, 12, 72),
            new THREE_NS.MeshStandardMaterial({ color: 0x8b6d49, emissive: 0x8b6d49, emissiveIntensity: 0.15 })
        );
        orbit.rotation.x = Math.PI / 2;
        orbit.position.set(0, 1.1, 0);
        orbit.userData = { spinZ: -0.22 };
        worldGroup.add(orbit);

        const accentLeft = new THREE_NS.Mesh(
            new THREE_NS.SphereGeometry(0.22, 18, 18),
            new THREE_NS.MeshStandardMaterial({ color: 0x6f8d58, emissive: 0x3f5f32, emissiveIntensity: 0.22 })
        );
        accentLeft.position.set(-2.1, 1.0, 1.4);
        accentLeft.userData = { bobAmplitude: 0.18, bobOffset: 0.0, baseY: 1.0 };
        worldGroup.add(accentLeft);

        const accentRight = accentLeft.clone();
        accentRight.position.set(2.3, 1.25, -1.3);
        accentRight.userData = { bobAmplitude: 0.16, bobOffset: 1.8, baseY: 1.25 };
        worldGroup.add(accentRight);

        camera.position.set(0, 4.9, 10.8);
        camera.lookAt(0, 1.6, 0);
    }

    function addMapStroke(points, color, y, opacity) {
        const geometry = new THREE_NS.BufferGeometry().setFromPoints(
            points.map((point) => new THREE_NS.Vector3(point.x, y, point.z))
        );
        const line = new THREE_NS.Line(
            geometry,
            new THREE_NS.LineBasicMaterial({
                color: color,
                transparent: true,
                opacity: opacity
            })
        );
        worldGroup.add(line);
    }

    function renderRegionalMapScene(state) {
        currentSceneKind = "REGIONAL_MAP";
        scene.background = new THREE_NS.Color(0xe6cfac);

        const board = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(34.0, 0.72, 22.0),
            new THREE_NS.MeshStandardMaterial({ color: 0x6c4b2d, roughness: 0.95, metalness: 0.04 })
        );
        board.position.set(0, -0.48, 0);
        worldGroup.add(board);

        const mapSheet = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(31.0, 0.16, 18.8),
            new THREE_NS.MeshStandardMaterial({ color: 0xd9bf90, roughness: 0.98, metalness: 0.01 })
        );
        mapSheet.position.set(0, -0.03, 0);
        worldGroup.add(mapSheet);

        const mapInset = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(29.8, 0.04, 17.6),
            new THREE_NS.MeshStandardMaterial({ color: 0xcfae78, roughness: 1.0, metalness: 0.0 })
        );
        mapInset.position.set(0, 0.06, 0);
        worldGroup.add(mapInset);

        const northMarker = new THREE_NS.Mesh(
            new THREE_NS.ConeGeometry(0.18, 0.6, 3),
            new THREE_NS.MeshStandardMaterial({ color: 0x53351b, roughness: 0.86, metalness: 0.05 })
        );
        northMarker.rotation.x = Math.PI;
        northMarker.position.set(13.5, 0.28, -7.6);
        worldGroup.add(northMarker);

        const positions = new Map();
        const scale = 0.03;
        const rawPositions = state.regionalMap.sites.map((site) => ({
            siteId: site.siteId,
            rawX: site.mapX * scale,
            rawZ: -site.mapY * scale
        }));

        if (rawPositions.length > 0) {
            const rawXs = rawPositions.map((entry) => entry.rawX);
            const rawZs = rawPositions.map((entry) => entry.rawZ);
            const minRawX = Math.min.apply(null, rawXs);
            const maxRawX = Math.max.apply(null, rawXs);
            const minRawZ = Math.min.apply(null, rawZs);
            const maxRawZ = Math.max.apply(null, rawZs);
            const rawCenterX = (minRawX + maxRawX) * 0.5;
            const rawCenterZ = (minRawZ + maxRawZ) * 0.5;
            const useCurvedFallback = (maxRawZ - minRawZ) < 0.4;

            if (useCurvedFallback) {
                const orderedSites = rawPositions.slice().sort((left, right) => left.rawX - right.rawX);
                const lastIndex = Math.max(orderedSites.length - 1, 1);

                orderedSites.forEach((entry, index) => {
                    const normalized = index / lastIndex;
                    const centered = normalized - 0.5;
                    positions.set(entry.siteId, {
                        x: centered * 14.0,
                        z: Math.sin(normalized * Math.PI) * 3.2 - 1.6 + Math.cos(normalized * Math.PI * 2.0) * 0.45
                    });
                });
            } else {
                rawPositions.forEach((entry) => {
                    positions.set(entry.siteId, {
                        x: entry.rawX - rawCenterX,
                        z: entry.rawZ - rawCenterZ
                    });
                });
            }
        }

        [
            [
                { x: -12.8, z: -6.4 },
                { x: -8.5, z: -5.6 },
                { x: -3.2, z: -6.0 },
                { x: 2.0, z: -5.0 },
                { x: 7.4, z: -5.6 },
                { x: 12.8, z: -4.9 }
            ],
            [
                { x: -13.4, z: -2.4 },
                { x: -9.1, z: -1.3 },
                { x: -4.8, z: -2.2 },
                { x: 0.8, z: -1.0 },
                { x: 6.5, z: -1.7 },
                { x: 13.1, z: -0.6 }
            ],
            [
                { x: -13.2, z: 1.8 },
                { x: -8.2, z: 2.6 },
                { x: -4.1, z: 1.4 },
                { x: 2.4, z: 2.2 },
                { x: 6.8, z: 1.1 },
                { x: 12.6, z: 2.0 }
            ],
            [
                { x: -12.4, z: 5.6 },
                { x: -7.2, z: 4.8 },
                { x: -2.6, z: 5.8 },
                { x: 2.8, z: 4.7 },
                { x: 8.0, z: 5.5 },
                { x: 12.0, z: 4.9 }
            ]
        ].forEach((stroke, index) => {
            addMapStroke(stroke, index % 2 === 0 ? 0xb38859 : 0xc49a67, 0.11, 0.62);
        });

        [
            [
                { x: -11.4, z: -7.0 },
                { x: -8.6, z: -3.2 },
                { x: -6.8, z: 0.4 },
                { x: -5.5, z: 4.2 },
                { x: -4.2, z: 7.0 }
            ],
            [
                { x: 1.0, z: -7.4 },
                { x: 0.4, z: -3.3 },
                { x: 0.8, z: 0.2 },
                { x: 1.6, z: 3.8 },
                { x: 2.8, z: 6.8 }
            ],
            [
                { x: 9.6, z: -7.2 },
                { x: 8.3, z: -4.0 },
                { x: 7.8, z: -0.5 },
                { x: 8.7, z: 3.1 },
                { x: 10.4, z: 6.6 }
            ]
        ].forEach((stroke) => {
            addMapStroke(stroke, 0xa77f50, 0.11, 0.36);
        });

        state.regionalMap.links.forEach((link) => {
            const from = positions.get(link.fromSiteId);
            const to = positions.get(link.toSiteId);
            if (!from || !to) {
                return;
            }

            const deltaX = to.x - from.x;
            const deltaZ = to.z - from.z;
            const length = Math.sqrt(deltaX * deltaX + deltaZ * deltaZ) || 1.0;
            const normalX = -deltaZ / length;
            const normalZ = deltaX / length;
            const midpoint = new THREE_NS.Vector3(
                (from.x + to.x) * 0.5 + normalX * 0.22,
                0.16,
                (from.z + to.z) * 0.5 + normalZ * 0.22
            );
            const curve = new THREE_NS.QuadraticBezierCurve3(
                new THREE_NS.Vector3(from.x, 0.15, from.z),
                midpoint,
                new THREE_NS.Vector3(to.x, 0.15, to.z)
            );
            const path = new THREE_NS.Mesh(
                new THREE_NS.TubeGeometry(curve, 18, 0.035, 8, false),
                new THREE_NS.MeshStandardMaterial({ color: 0x725233, roughness: 0.86, metalness: 0.03 })
            );
            worldGroup.add(path);
        });

        state.regionalMap.sites.forEach((site) => {
            const position = positions.get(site.siteId);
            if (!position) {
                return;
            }

            const markerGroup = new THREE_NS.Group();
            markerGroup.position.set(position.x, 0.0, position.z);

            const palette =
                site.siteState === "AVAILABLE"
                    ? { plinth: 0x816346, badge: 0x6f8756, cloth: 0x99ad74 }
                    : site.siteState === "COMPLETED"
                        ? { plinth: 0x785736, badge: 0xb18755, cloth: 0xcab47b }
                        : { plinth: 0x6d6156, badge: 0x918474, cloth: 0x918474 };

            const plinth = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.72, 0.82, 0.18, 24),
                new THREE_NS.MeshStandardMaterial({ color: palette.plinth, roughness: 0.9, metalness: 0.05 })
            );
            plinth.position.y = 0.12;
            plinth.userData = { siteId: site.siteId, siteState: site.siteState };
            markerGroup.add(plinth);

            const badge = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.44, 0.5, 0.12, 18),
                new THREE_NS.MeshStandardMaterial({ color: palette.badge, roughness: 0.82, metalness: 0.06 })
            );
            badge.position.y = 0.27;
            markerGroup.add(badge);

            const post = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.04, 0.04, 0.4, 8),
                new THREE_NS.MeshStandardMaterial({ color: 0x3f2c18, roughness: 0.88, metalness: 0.04 })
            );
            post.position.y = 0.5;
            markerGroup.add(post);

            if (site.siteState !== "LOCKED") {
                const pennant = new THREE_NS.Mesh(
                    new THREE_NS.BoxGeometry(0.12, 0.28, 0.02),
                    new THREE_NS.MeshStandardMaterial({ color: palette.cloth, roughness: 0.82, metalness: 0.02 })
                );
                pennant.position.set(0.13, 0.64, 0.0);
                pennant.rotation.z = -0.18;
                markerGroup.add(pennant);
            }

            if (site.siteState === "AVAILABLE") {
                mapPickables.push(plinth);
            }

            const isSelected = state.selectedSiteId === site.siteId || (site.flags & 1) !== 0;
            if (isSelected) {
                const selectedRing = new THREE_NS.Mesh(
                    new THREE_NS.TorusGeometry(1.04, 0.05, 10, 48),
                    new THREE_NS.MeshStandardMaterial({ color: 0xcfb07a, roughness: 0.72, metalness: 0.18 })
                );
                selectedRing.rotation.x = Math.PI / 2;
                selectedRing.position.y = 0.16;
                markerGroup.add(selectedRing);

                [
                    { x: -0.84, z: -0.84 },
                    { x: 0.84, z: -0.84 },
                    { x: -0.84, z: 0.84 },
                    { x: 0.84, z: 0.84 }
                ].forEach((corner) => {
                    const marker = new THREE_NS.Mesh(
                        new THREE_NS.BoxGeometry(0.18, 0.06, 0.42),
                        new THREE_NS.MeshStandardMaterial({ color: 0x5a4126, roughness: 0.88, metalness: 0.04 })
                    );
                    marker.position.set(corner.x, 0.15, corner.z);
                    marker.rotation.y = Math.atan2(corner.x, corner.z);
                    markerGroup.add(marker);
                });
            }

            worldGroup.add(markerGroup);
        });

        if (state.regionalMap.sites.length > 0) {
            const xs = state.regionalMap.sites.map((site) => positions.get(site.siteId).x);
            const zs = state.regionalMap.sites.map((site) => positions.get(site.siteId).z);
            const centerX = (Math.min.apply(null, xs) + Math.max.apply(null, xs)) * 0.5;
            const centerZ = (Math.min.apply(null, zs) + Math.max.apply(null, zs)) * 0.5;
            camera.position.set(centerX + 7.2, 11.2, centerZ + 8.9);
            camera.lookAt(centerX, 0.25, centerZ);
        } else {
            camera.position.set(6.6, 10.5, 8.0);
            camera.lookAt(0, 0.2, 0);
        }
    }

    function clamp01(value) {
        return Math.max(0, Math.min(value, 1));
    }

    function lerp(left, right, amount) {
        return left + (right - left) * amount;
    }

    function createCapsulePart(radius, totalLength, material, capSegments, radialSegments) {
        const cylinderLength = Math.max(totalLength - radius * 2.0, 0.01);
        return new THREE_NS.Mesh(
            new THREE_NS.CapsuleGeometry(radius, cylinderLength, capSegments || 4, radialSegments || 8),
            material
        );
    }

    function createLimbChain(options) {
        const root = new THREE_NS.Group();
        const joint = new THREE_NS.Group();
        const end = new THREE_NS.Group();

        const upper = createCapsulePart(options.upperRadius, options.upperLength, options.upperMaterial, 3, 8);
        upper.position.y = -options.upperLength * 0.5;
        root.add(upper);

        joint.position.y = -options.upperLength;
        root.add(joint);

        const lower = createCapsulePart(options.lowerRadius, options.lowerLength, options.lowerMaterial, 3, 8);
        lower.position.y = -options.lowerLength * 0.5;
        joint.add(lower);

        end.position.y = -options.lowerLength;
        joint.add(end);

        if (options.endMesh) {
            end.add(options.endMesh);
        }

        return {
            root: root,
            joint: joint,
            end: end
        };
    }

    function createHumanoidWorker() {
        const workerGroup = new THREE_NS.Group();
        const rigRoot = new THREE_NS.Group();
        workerGroup.add(rigRoot);

        const materials = {
            jacket: new THREE_NS.MeshStandardMaterial({ color: 0x2e6965, roughness: 0.76, metalness: 0.04 }),
            shirt: new THREE_NS.MeshStandardMaterial({ color: 0xd7c7a7, roughness: 0.88, metalness: 0.01 }),
            pants: new THREE_NS.MeshStandardMaterial({ color: 0x60513e, roughness: 0.82, metalness: 0.03 }),
            boot: new THREE_NS.MeshStandardMaterial({ color: 0x29221a, roughness: 0.72, metalness: 0.06 }),
            skin: new THREE_NS.MeshStandardMaterial({ color: 0xe2c59f, roughness: 0.74, metalness: 0.01 }),
            hat: new THREE_NS.MeshStandardMaterial({ color: 0xb99a6b, roughness: 0.86, metalness: 0.02 }),
            pack: new THREE_NS.MeshStandardMaterial({ color: 0x6b5637, roughness: 0.84, metalness: 0.04 }),
            detail: new THREE_NS.MeshStandardMaterial({ color: 0x172b2d, roughness: 0.66, metalness: 0.07 })
        };

        const shadow = new THREE_NS.Mesh(
            new THREE_NS.CircleGeometry(0.42, 24),
            new THREE_NS.MeshBasicMaterial({ color: 0x3d2f1f, transparent: true, opacity: 0.18, depthWrite: false })
        );
        shadow.rotation.x = -Math.PI / 2;
        shadow.scale.y = 0.56;
        shadow.position.y = 0.018;
        workerGroup.add(shadow);

        const pelvis = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(0.34, 0.18, 0.22),
            materials.pants
        );
        pelvis.position.y = 0.68;
        rigRoot.add(pelvis);

        const torso = new THREE_NS.Group();
        torso.position.y = 0.72;
        rigRoot.add(torso);

        const torsoCore = createCapsulePart(0.18, 0.58, materials.jacket, 5, 10);
        torsoCore.position.y = 0.28;
        torso.add(torsoCore);

        const chestPanel = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(0.20, 0.28, 0.024),
            materials.shirt
        );
        chestPanel.position.set(0, 0.30, 0.174);
        torso.add(chestPanel);

        const backpack = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(0.28, 0.38, 0.12),
            materials.pack
        );
        backpack.position.set(0, 0.24, -0.19);
        torso.add(backpack);

        const neck = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.055, 0.065, 0.10, 10),
            materials.skin
        );
        neck.position.y = 0.62;
        torso.add(neck);

        const head = new THREE_NS.Group();
        head.position.y = 0.76;
        torso.add(head);

        const skull = new THREE_NS.Mesh(
            new THREE_NS.SphereGeometry(0.145, 18, 16),
            materials.skin
        );
        head.add(skull);

        const nose = new THREE_NS.Mesh(
            new THREE_NS.ConeGeometry(0.025, 0.075, 6),
            materials.skin
        );
        nose.position.set(0, 0.0, 0.142);
        nose.rotation.x = Math.PI / 2;
        head.add(nose);

        const eyeBar = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(0.13, 0.022, 0.012),
            materials.detail
        );
        eyeBar.position.set(0, 0.045, 0.133);
        head.add(eyeBar);

        const hatBrim = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.19, 0.19, 0.028, 22),
            materials.hat
        );
        hatBrim.position.y = 0.13;
        head.add(hatBrim);

        const hatCrown = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.115, 0.135, 0.11, 18),
            materials.hat
        );
        hatCrown.position.y = 0.20;
        head.add(hatCrown);

        const chestLight = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.028, 0.028, 0.014, 12),
            new THREE_NS.MeshStandardMaterial({ color: 0xc6e0d4, emissive: 0x4f8c7d, emissiveIntensity: 0.22 })
        );
        chestLight.position.set(0.055, 0.37, 0.192);
        chestLight.rotation.x = Math.PI / 2;
        torso.add(chestLight);

        function createBoot(side) {
            const boot = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.12, 0.075, 0.24),
                materials.boot
            );
            boot.position.set(side * 0.01, -0.02, 0.075);
            return boot;
        }

        function createHand() {
            const hand = new THREE_NS.Mesh(
                new THREE_NS.SphereGeometry(0.055, 10, 8),
                materials.skin
            );
            hand.position.y = -0.02;
            return hand;
        }

        const leftLeg = createLimbChain({
            upperLength: 0.32,
            lowerLength: 0.33,
            upperRadius: 0.06,
            lowerRadius: 0.052,
            upperMaterial: materials.pants,
            lowerMaterial: materials.pants,
            endMesh: createBoot(-1)
        });
        leftLeg.root.position.set(-0.095, 0.67, 0.015);
        rigRoot.add(leftLeg.root);

        const rightLeg = createLimbChain({
            upperLength: 0.32,
            lowerLength: 0.33,
            upperRadius: 0.06,
            lowerRadius: 0.052,
            upperMaterial: materials.pants,
            lowerMaterial: materials.pants,
            endMesh: createBoot(1)
        });
        rightLeg.root.position.set(0.095, 0.67, 0.015);
        rigRoot.add(rightLeg.root);

        const leftArm = createLimbChain({
            upperLength: 0.29,
            lowerLength: 0.27,
            upperRadius: 0.046,
            lowerRadius: 0.04,
            upperMaterial: materials.jacket,
            lowerMaterial: materials.jacket,
            endMesh: createHand()
        });
        leftArm.root.position.set(-0.245, 0.53, 0.0);
        torso.add(leftArm.root);

        const rightArm = createLimbChain({
            upperLength: 0.29,
            lowerLength: 0.27,
            upperRadius: 0.046,
            lowerRadius: 0.04,
            upperMaterial: materials.jacket,
            lowerMaterial: materials.jacket,
            endMesh: createHand()
        });
        rightArm.root.position.set(0.245, 0.53, 0.0);
        torso.add(rightArm.root);

        return {
            group: workerGroup,
            rig: {
                root: rigRoot,
                torso: torso,
                pelvis: pelvis,
                head: head,
                leftHip: leftLeg.root,
                rightHip: rightLeg.root,
                leftKnee: leftLeg.joint,
                rightKnee: rightLeg.joint,
                leftAnkle: leftLeg.end,
                rightAnkle: rightLeg.end,
                leftShoulder: leftArm.root,
                rightShoulder: rightArm.root,
                leftElbow: leftArm.joint,
                rightElbow: rightArm.joint,
                leftWrist: leftArm.end,
                rightWrist: rightArm.end,
                shadow: shadow
            }
        };
    }

    function updateHumanoidWorkerAnimation(cache, deltaSeconds, elapsed, movementSpeed, distanceToTarget, currentActionKind) {
        const rig = cache.workerRig;
        if (!rig) {
            return;
        }

        const locomotionAmount = clamp01((Math.max(movementSpeed, distanceToTarget * 8.0) - 0.04) / 0.85);
        const runAmount = clamp01((movementSpeed - 1.55) / 1.35);
        const cadence = lerp(5.4, 9.7, runAmount) * locomotionAmount;
        cache.workerAnimPhase += cadence * deltaSeconds;

        const phase = cache.workerAnimPhase;
        const leftStep = Math.sin(phase);
        const rightStep = -leftStep;
        const sideSway = Math.sin(phase * 2.0);
        const lift = Math.abs(Math.cos(phase));
        const legSwing = lerp(0.42, 0.78, runAmount) * locomotionAmount;
        const armSwing = lerp(0.48, 0.92, runAmount) * locomotionAmount;
        const kneeBend = lerp(0.46, 0.92, runAmount) * locomotionAmount;
        const idleBreath = Math.sin(elapsed * 1.55);

        rig.root.position.y = lift * lerp(0.018, 0.052, runAmount) * locomotionAmount +
            idleBreath * 0.006 * (1.0 - locomotionAmount);
        rig.root.rotation.z = sideSway * lerp(0.012, 0.026, runAmount) * locomotionAmount;

        rig.torso.rotation.x = -lerp(0.025, 0.13, runAmount) * locomotionAmount;
        rig.torso.rotation.z = -sideSway * lerp(0.026, 0.060, runAmount) * locomotionAmount;
        rig.pelvis.rotation.z = sideSway * lerp(0.018, 0.040, runAmount) * locomotionAmount;

        rig.leftHip.rotation.set(leftStep * legSwing - 0.02 * locomotionAmount, 0, -0.045);
        rig.rightHip.rotation.set(rightStep * legSwing - 0.02 * locomotionAmount, 0, 0.045);
        rig.leftKnee.rotation.x = 0.045 + Math.max(0, -leftStep) * kneeBend;
        rig.rightKnee.rotation.x = 0.045 + Math.max(0, -rightStep) * kneeBend;
        rig.leftAnkle.rotation.x = -leftStep * 0.22 * locomotionAmount - rig.leftKnee.rotation.x * 0.18;
        rig.rightAnkle.rotation.x = -rightStep * 0.22 * locomotionAmount - rig.rightKnee.rotation.x * 0.18;

        rig.leftShoulder.rotation.set(-leftStep * armSwing + 0.05 * locomotionAmount, 0, -0.18);
        rig.rightShoulder.rotation.set(-rightStep * armSwing + 0.05 * locomotionAmount, 0, 0.18);
        rig.leftElbow.rotation.x = 0.16 + locomotionAmount * lerp(0.08, 0.36, runAmount) +
            Math.max(0, leftStep) * 0.14 * locomotionAmount;
        rig.rightElbow.rotation.x = 0.16 + locomotionAmount * lerp(0.08, 0.36, runAmount) +
            Math.max(0, rightStep) * 0.14 * locomotionAmount;
        rig.leftWrist.rotation.x = -0.08 + Math.max(0, -leftStep) * 0.12 * locomotionAmount;
        rig.rightWrist.rotation.x = -0.08 + Math.max(0, -rightStep) * 0.12 * locomotionAmount;

        rig.head.rotation.x = -rig.torso.rotation.x * 0.55 + idleBreath * 0.012 * (1.0 - locomotionAmount);
        rig.head.rotation.z = -rig.torso.rotation.z * 0.65;
        rig.shadow.scale.set(
            lerp(1.0, 1.16, locomotionAmount),
            lerp(0.56, 0.70, locomotionAmount),
            1.0
        );

        const plantingBlend =
            (currentActionKind === 1 || currentActionKind === 2 || currentActionKind === 6)
                ? clamp01(1.0 - locomotionAmount * 1.8)
                : 0.0;
        if (plantingBlend > 0.0) {
            cache.workerActionPhase += deltaSeconds * 4.8;
            const plantingPhase = Math.sin(cache.workerActionPhase);
            rig.root.position.y += (0.018 + Math.abs(plantingPhase) * 0.02) * plantingBlend;
            rig.torso.rotation.x = lerp(rig.torso.rotation.x, 0.34 + plantingPhase * 0.04, plantingBlend);
            rig.pelvis.rotation.z = lerp(rig.pelvis.rotation.z, -0.03, plantingBlend);
            rig.leftShoulder.rotation.set(
                lerp(rig.leftShoulder.rotation.x, -1.0 + plantingPhase * 0.12, plantingBlend),
                0.14 * plantingBlend,
                lerp(rig.leftShoulder.rotation.z, -0.22, plantingBlend)
            );
            rig.rightShoulder.rotation.set(
                lerp(rig.rightShoulder.rotation.x, -0.72 - plantingPhase * 0.18, plantingBlend),
                -0.16 * plantingBlend,
                lerp(rig.rightShoulder.rotation.z, 0.22, plantingBlend)
            );
            rig.leftElbow.rotation.x = lerp(rig.leftElbow.rotation.x, 0.72 + Math.max(0, -plantingPhase) * 0.18, plantingBlend);
            rig.rightElbow.rotation.x = lerp(rig.rightElbow.rotation.x, 1.02 + Math.max(0, plantingPhase) * 0.22, plantingBlend);
            rig.leftWrist.rotation.x = lerp(rig.leftWrist.rotation.x, -0.18 + plantingPhase * 0.08, plantingBlend);
            rig.rightWrist.rotation.x = lerp(rig.rightWrist.rotation.x, -0.34 - plantingPhase * 0.12, plantingBlend);
            rig.head.rotation.x = lerp(rig.head.rotation.x, 0.12, plantingBlend);
            rig.shadow.scale.set(
                lerp(rig.shadow.scale.x, 0.94, plantingBlend),
                lerp(rig.shadow.scale.y, 0.62, plantingBlend),
                1.0
            );
        }

        const consumeBlend =
            (currentActionKind === 7 || currentActionKind === 8)
                ? clamp01(1.0 - locomotionAmount * 1.8)
                : 0.0;
        if (consumeBlend > 0.0) {
            cache.workerActionPhase += deltaSeconds * (currentActionKind === 7 ? 5.2 : 4.2);
            const consumePhase = Math.sin(cache.workerActionPhase);
            const sipPulse = 0.5 + 0.5 * Math.sin(cache.workerActionPhase * 2.0);
            rig.root.position.y += (0.01 + sipPulse * 0.01) * consumeBlend;
            rig.torso.rotation.x = lerp(rig.torso.rotation.x, 0.12 + consumePhase * 0.03, consumeBlend);
            rig.torso.rotation.z = lerp(rig.torso.rotation.z, currentActionKind === 7 ? 0.05 : -0.04, consumeBlend);
            rig.pelvis.rotation.z = lerp(rig.pelvis.rotation.z, currentActionKind === 7 ? -0.02 : 0.02, consumeBlend);
            rig.leftShoulder.rotation.set(
                lerp(rig.leftShoulder.rotation.x, currentActionKind === 7 ? -0.38 : -0.72, consumeBlend),
                lerp(rig.leftShoulder.rotation.y, currentActionKind === 7 ? 0.08 : 0.16, consumeBlend),
                lerp(rig.leftShoulder.rotation.z, -0.18, consumeBlend)
            );
            rig.rightShoulder.rotation.set(
                lerp(rig.rightShoulder.rotation.x, currentActionKind === 7 ? -1.26 + consumePhase * 0.08 : -1.08 + consumePhase * 0.06, consumeBlend),
                lerp(rig.rightShoulder.rotation.y, currentActionKind === 7 ? -0.24 : -0.08, consumeBlend),
                lerp(rig.rightShoulder.rotation.z, 0.24, consumeBlend)
            );
            rig.leftElbow.rotation.x = lerp(rig.leftElbow.rotation.x, currentActionKind === 7 ? 0.62 : 0.96, consumeBlend);
            rig.rightElbow.rotation.x = lerp(rig.rightElbow.rotation.x, currentActionKind === 7 ? 1.48 + sipPulse * 0.08 : 1.34 + sipPulse * 0.06, consumeBlend);
            rig.leftWrist.rotation.x = lerp(rig.leftWrist.rotation.x, currentActionKind === 7 ? -0.14 : -0.28, consumeBlend);
            rig.rightWrist.rotation.x = lerp(rig.rightWrist.rotation.x, currentActionKind === 7 ? -0.92 : -0.58, consumeBlend);
            rig.head.rotation.x = lerp(rig.head.rotation.x, currentActionKind === 7 ? 0.26 : 0.18, consumeBlend);
            rig.head.rotation.z = lerp(rig.head.rotation.z, currentActionKind === 7 ? -0.06 : 0.04, consumeBlend);
            rig.shadow.scale.set(
                lerp(rig.shadow.scale.x, 0.97, consumeBlend),
                lerp(rig.shadow.scale.y, 0.60, consumeBlend),
                1.0
            );
        }
    }

    function buildSiteBootstrapSignature(siteBootstrap) {
        return JSON.stringify(
            {
                camp: siteBootstrap.camp
                    ? [siteBootstrap.camp.tileX, siteBootstrap.camp.tileY]
                    : null,
                tiles: siteBootstrap.tiles.map((tile) => [
                tile.x,
                tile.y,
                tile.terrainTypeId,
                tile.plantTypeId,
                tile.structureTypeId,
                tile.groundCoverTypeId,
                Math.round((tile.plantDensity || 0) * 100),
                Math.round((tile.sandBurial || 0) * 100)
                ])
            }
        );
    }

    function getSiteCameraOffsetYForGroundDistance(groundDistance) {
        return siteCameraDefaults.lookHeight +
            groundDistance * Math.tan(siteCameraDefaults.viewAngleRadians);
    }

    function buildSiteCameraOrbitState(siteBootstrap, width, height, previousCache) {
        let orbitDirectionX = siteCameraDefaults.offsetX;
        let orbitDirectionZ = siteCameraDefaults.offsetZ;
        let orbitDistance = siteCameraDefaults.nearestGroundDistance;

        if (previousCache && previousCache.siteId === siteBootstrap.siteId) {
            orbitDirectionX = previousCache.cameraOrbitOffsetX;
            orbitDirectionZ = previousCache.cameraOrbitOffsetZ;
            if (typeof previousCache.cameraOrbitGroundDistance === "number") {
                orbitDistance = previousCache.cameraOrbitGroundDistance;
            }
        }

        const orbitDirectionLength = Math.hypot(orbitDirectionX, orbitDirectionZ);
        if (orbitDirectionLength <= 0.000001) {
            orbitDirectionX = 0.0;
            orbitDirectionZ = 1.0;
        } else {
            orbitDirectionX /= orbitDirectionLength;
            orbitDirectionZ /= orbitDirectionLength;
        }

        orbitDistance = Math.max(
            siteCameraDefaults.nearestGroundDistance,
            Math.min(siteCameraDefaults.farthestGroundDistance, orbitDistance)
        );

        return {
            offsetX: orbitDirectionX * orbitDistance,
            offsetY: getSiteCameraOffsetYForGroundDistance(orbitDistance),
            offsetZ: orbitDirectionZ * orbitDistance,
            groundDistance: orbitDistance
        };
    }

    function rotateSiteCameraOrbit(cache, angleRadians) {
        if (!cache || Math.abs(angleRadians) <= 0.000001) {
            return;
        }

        const cosAngle = Math.cos(angleRadians);
        const sinAngle = Math.sin(angleRadians);
        const previousOffsetX = cache.cameraOrbitOffsetX;
        const previousOffsetZ = cache.cameraOrbitOffsetZ;
        cache.cameraOrbitOffsetX = previousOffsetX * cosAngle - previousOffsetZ * sinAngle;
        cache.cameraOrbitOffsetZ = previousOffsetX * sinAngle + previousOffsetZ * cosAngle;
    }

    function setSiteCameraOrbitDistance(cache, groundDistance) {
        if (!cache) {
            return;
        }

        const clampedDistance = Math.max(
            siteCameraDefaults.nearestGroundDistance,
            Math.min(siteCameraDefaults.farthestGroundDistance, groundDistance)
        );
        let orbitDirectionX = cache.cameraOrbitOffsetX;
        let orbitDirectionZ = cache.cameraOrbitOffsetZ;
        const orbitDirectionLength = Math.hypot(orbitDirectionX, orbitDirectionZ);
        if (orbitDirectionLength <= 0.000001) {
            orbitDirectionX = 0.0;
            orbitDirectionZ = 1.0;
        } else {
            orbitDirectionX /= orbitDirectionLength;
            orbitDirectionZ /= orbitDirectionLength;
        }

        cache.cameraOrbitGroundDistance = clampedDistance;
        cache.cameraOrbitOffsetX = orbitDirectionX * clampedDistance;
        cache.cameraOrbitOffsetY = getSiteCameraOffsetYForGroundDistance(clampedDistance);
        cache.cameraOrbitOffsetZ = orbitDirectionZ * clampedDistance;
    }

    function updateSiteCameraZoomFromWheel(deltaY) {
        if (!siteSceneCache || Math.abs(deltaY) <= 0.000001) {
            return;
        }

        const zoomDirection = Math.sign(deltaY);
        if (zoomDirection === 0) {
            return;
        }

        setSiteCameraOrbitDistance(
            siteSceneCache,
            siteSceneCache.cameraOrbitGroundDistance +
                zoomDirection * siteCameraDefaults.wheelZoomStepDistance
        );
    }

    function updateSiteWorkerTargetFromState(siteState, frameNumber) {
        if (!siteSceneCache) {
            return;
        }

        let cameraTargetX = 0;
        let cameraTargetZ = 0;
        if (siteState && siteState.worker) {
            cameraTargetX = siteState.worker.tileX - siteSceneCache.offsetX;
            cameraTargetZ = siteState.worker.tileY - siteSceneCache.offsetZ;
            const nowMs = performance.now();
            if (siteSceneCache.workerTargetInitialized) {
                const targetDistance = Math.hypot(
                    cameraTargetX - siteSceneCache.lastWorkerTargetX,
                    cameraTargetZ - siteSceneCache.lastWorkerTargetZ
                );
                const patchFrameDelta =
                    siteSceneCache.lastWorkerTargetFrameNumber > 0
                        ? Math.max(frameNumber - siteSceneCache.lastWorkerTargetFrameNumber, 1)
                        : 1;
                siteSceneCache.lastWorkerPatchDistance = targetDistance;
                siteSceneCache.lastWorkerPatchDeltaFrames = patchFrameDelta;
                siteSceneCache.lastWorkerPatchIntervalMs =
                    siteSceneCache.lastWorkerPatchTimeMs > 0
                        ? Math.max(nowMs - siteSceneCache.lastWorkerPatchTimeMs, 0.0)
                        : 0.0;
                if (targetDistance > 0.0001) {
                    const effectiveFrameDelta =
                        siteSceneCache.workerAuthoritativeSpeed > 0.0001
                            ? patchFrameDelta
                            : 1;
                    siteSceneCache.workerAuthoritativeSpeed =
                        targetDistance / (effectiveFrameDelta * runtimeFixedStepSeconds);
                } else {
                    siteSceneCache.workerAuthoritativeSpeed = 0.0;
                }
            } else {
                siteSceneCache.lastWorkerPatchDistance = 0.0;
                siteSceneCache.lastWorkerPatchDeltaFrames = 0;
                siteSceneCache.lastWorkerPatchIntervalMs = 0.0;
            }
            siteSceneCache.workerTargetInitialized = true;
            siteSceneCache.lastWorkerPatchTimeMs = nowMs;
            siteSceneCache.lastWorkerTargetFrameNumber = frameNumber;
            siteSceneCache.lastWorkerTargetX = cameraTargetX;
            siteSceneCache.lastWorkerTargetZ = cameraTargetZ;
            siteSceneCache.workerTargetX = cameraTargetX;
            siteSceneCache.workerTargetZ = cameraTargetZ;
            siteSceneCache.workerTargetYaw = (siteState.worker.facingDegrees || 0) * Math.PI / 180.0;

            if (!siteSceneCache.workerInitialized) {
                siteSceneCache.workerGroup.position.set(cameraTargetX, 0.0, cameraTargetZ);
                siteSceneCache.workerGroup.rotation.y = siteSceneCache.workerTargetYaw;
                siteSceneCache.workerInitialized = true;
                siteSceneCache.workerVisualSpeed = 0.0;
            }
        }

        siteSceneCache.cameraTargetX = cameraTargetX;
        siteSceneCache.cameraTargetZ = cameraTargetZ;
    }

    function applySiteCameraTransform(cache, focusX, focusZ, blendFactor) {
        if (!cache) {
            return;
        }

        const desiredCameraX = focusX + cache.cameraOrbitOffsetX;
        const desiredCameraY = cache.cameraOrbitOffsetY;
        const desiredCameraZ = focusZ + cache.cameraOrbitOffsetZ;
        if (blendFactor >= 1.0) {
            camera.position.set(desiredCameraX, desiredCameraY, desiredCameraZ);
        } else {
            camera.position.x += (desiredCameraX - camera.position.x) * blendFactor;
            camera.position.y += (desiredCameraY - camera.position.y) * blendFactor;
            camera.position.z += (desiredCameraZ - camera.position.z) * blendFactor;
        }

        camera.lookAt(focusX, siteCameraDefaults.lookHeight, focusZ);
    }

    function stopCameraOrbitDrag(pointerId) {
        if (cameraOrbitDragPointerId == null) {
            return;
        }
        if (pointerId != null && pointerId !== cameraOrbitDragPointerId) {
            return;
        }

        if (renderer.domElement.hasPointerCapture(cameraOrbitDragPointerId)) {
            renderer.domElement.releasePointerCapture(cameraOrbitDragPointerId);
        }

        cameraOrbitDragPointerId = null;
        cameraOrbitDragButton = -1;
        cameraOrbitDragLastClientX = 0;
    }

    function releasePointerCaptureIfHeld(pointerId) {
        if (pointerId == null) {
            return;
        }
        if (renderer.domElement.hasPointerCapture(pointerId)) {
            renderer.domElement.releasePointerCapture(pointerId);
        }
    }

    function clearPendingSiteOrbitGesture(pointerId) {
        if (!pendingSiteOrbitGesture) {
            return;
        }
        if (pointerId != null && pointerId !== pendingSiteOrbitGesture.pointerId) {
            return;
        }

        releasePointerCaptureIfHeld(pendingSiteOrbitGesture.pointerId);
        pendingSiteOrbitGesture = null;
    }

    function beginPendingSiteOrbitGesture(event) {
        clearPendingSiteOrbitGesture();
        pendingSiteOrbitGesture = {
            pointerId: event.pointerId,
            startClientX: event.clientX,
            startClientY: event.clientY,
            dragging: false
        };
        renderer.domElement.setPointerCapture(event.pointerId);
    }

    function beginSiteCameraOrbitDrag(pointerId, clientX) {
        cameraOrbitDragPointerId = pointerId;
        cameraOrbitDragButton = orbitMouseButton;
        cameraOrbitDragLastClientX = clientX;
        closeTileContextMenu();
    }

    function shouldSuppressSiteContextMenu() {
        return isSiteActiveView();
    }

    function isSiteActiveView() {
        return !!latestState && latestState.appState === "SITE_ACTIVE";
    }

    function getSiteOrbitDragButtonFromEvent(event) {
        if (!isSiteActiveView()) {
            return -1;
        }

        if (event.button === orbitMouseButton) {
            return orbitMouseButton;
        }

        return -1;
    }

    function isSiteOrbitMouseEvent(event) {
        if (!isSiteActiveView()) {
            return false;
        }

        if (getSiteOrbitDragButtonFromEvent(event) >= 0) {
            return true;
        }

        if (cameraOrbitDragButton === orbitMouseButton) {
            return (event.buttons & orbitMouseButtonsMask) !== 0;
        }

        return false;
    }

    function isSiteCameraOrbitActive() {
        return cameraOrbitDragPointerId != null;
    }

    function isSiteCameraPointerLocked() {
        return document.pointerLockElement === renderer.domElement;
    }

    function updateCameraOrbitFromDelta(deltaX) {
        if (!siteSceneCache || Math.abs(deltaX) <= 0.000001) {
            return;
        }

        rotateSiteCameraOrbit(siteSceneCache, -deltaX * siteCameraDefaults.rotateRadiansPerPixel);
        sendSiteControlState();
    }

    function tryRequestCameraPointerLock() {
        if (typeof renderer.domElement.requestPointerLock !== "function") {
            return;
        }

        try {
            renderer.domElement.requestPointerLock();
        } catch (_error) {
        }
    }

    function releaseCameraPointerLock() {
        if (!isSiteCameraPointerLocked() || typeof document.exitPointerLock !== "function") {
            return;
        }

        try {
            document.exitPointerLock();
        } catch (_error) {
        }
    }

    function deterministicNoise01(x, y, salt) {
        const value = Math.sin(x * 127.1 + y * 311.7 + salt * 74.7) * 43758.5453123;
        return value - Math.floor(value);
    }

    function createPlantMaterial(color, roughness, emissive, emissiveIntensity) {
        return new THREE_NS.MeshStandardMaterial({
            color: color,
            roughness: roughness == null ? 0.84 : roughness,
            metalness: 0.01,
            emissive: emissive == null ? 0x000000 : emissive,
            emissiveIntensity: emissiveIntensity == null ? 0.0 : emissiveIntensity
        });
    }

    function createLeafMesh(color, width, thickness, length) {
        const leaf = new THREE_NS.Mesh(
            new THREE_NS.SphereGeometry(1, 10, 10),
            createPlantMaterial(color, 0.78, color, 0.04)
        );
        leaf.scale.set(width, thickness, length);
        return leaf;
    }

    function addWindReedPlant(plantGroup, tile, plantDensity) {
        const stemColor = 0x6b8b4c;
        const leafColor = 0x93b868;
        const bunchCount = 5 + Math.round(plantDensity * 3);
        const baseHeight = 0.34 + plantDensity * 0.38;

        for (let index = 0; index < bunchCount; index += 1) {
            const angle = (index / bunchCount) * Math.PI * 2 + deterministicNoise01(tile.x, tile.y, index + 1) * 0.42;
            const offsetRadius = 0.03 + deterministicNoise01(tile.y, tile.x, index + 11) * 0.06;
            const stemHeight = baseHeight * (0.72 + deterministicNoise01(tile.x, tile.y, index + 21) * 0.38);
            const stemRadius = 0.012 + deterministicNoise01(tile.y, tile.x, index + 31) * 0.008;
            const stem = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(stemRadius * 0.55, stemRadius, stemHeight, 5),
                createPlantMaterial(stemColor)
            );
            stem.position.set(
                Math.cos(angle) * offsetRadius,
                stemHeight * 0.5,
                Math.sin(angle) * offsetRadius
            );
            stem.rotation.z = Math.cos(angle) * 0.2;
            stem.rotation.x = -Math.sin(angle) * 0.18;
            plantGroup.add(stem);

            const blade = createLeafMesh(leafColor, 0.02, 0.009, 0.14 + plantDensity * 0.1);
            blade.position.set(
                stem.position.x * 0.6,
                stemHeight * (0.56 + deterministicNoise01(tile.x, tile.y, index + 41) * 0.12),
                stem.position.z * 0.6
            );
            blade.rotation.y = angle;
            blade.rotation.z = 0.48 + deterministicNoise01(tile.y, tile.x, index + 51) * 0.44;
            plantGroup.add(blade);
        }
    }

    function addSaltbushPlant(plantGroup, tile, plantDensity) {
        const twigColor = 0x6b5c3b;
        const leafColors = [0x88a45d, 0x95b568, 0x78914d];
        const branchCount = 4 + Math.round(plantDensity * 2);
        const bushRadius = 0.2 + plantDensity * 0.14;

        for (let index = 0; index < branchCount; index += 1) {
            const angle = (index / branchCount) * Math.PI * 2 + deterministicNoise01(tile.x, tile.y, index + 61) * 0.6;
            const branchHeight = 0.12 + plantDensity * 0.12 + deterministicNoise01(tile.y, tile.x, index + 71) * 0.08;
            const branch = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.015, 0.022, branchHeight, 5),
                createPlantMaterial(twigColor, 0.9)
            );
            branch.position.set(0, branchHeight * 0.38, 0);
            branch.rotation.z = Math.cos(angle) * 0.65;
            branch.rotation.x = -Math.sin(angle) * 0.65;
            plantGroup.add(branch);
        }

        const clumpCount = 7 + Math.round(plantDensity * 4);
        for (let index = 0; index < clumpCount; index += 1) {
            const angle = deterministicNoise01(tile.x, tile.y, index + 81) * Math.PI * 2;
            const radius = bushRadius * (0.28 + deterministicNoise01(tile.y, tile.x, index + 91) * 0.72);
            const clump = new THREE_NS.Mesh(
                new THREE_NS.SphereGeometry(0.09 + deterministicNoise01(tile.x, tile.y, index + 101) * 0.03, 10, 10),
                createPlantMaterial(leafColors[index % leafColors.length], 0.8, 0x42522a, 0.04)
            );
            clump.scale.set(
                0.95 + deterministicNoise01(tile.x, tile.y, index + 111) * 0.3,
                0.7 + deterministicNoise01(tile.y, tile.x, index + 121) * 0.25,
                0.95 + deterministicNoise01(tile.x, tile.y, index + 131) * 0.3
            );
            clump.position.set(
                Math.cos(angle) * radius,
                0.12 + plantDensity * 0.08 + deterministicNoise01(tile.y, tile.x, index + 141) * 0.12,
                Math.sin(angle) * radius
            );
            plantGroup.add(clump);
        }
    }

    function addShadeCactusPlant(plantGroup, tile, plantDensity) {
        const bodyMaterial = createPlantMaterial(0x6b8c52, 0.82, 0x30411e, 0.06);
        const height = 0.48 + plantDensity * 0.42;
        const trunk = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.09, 0.12, height, 8),
            bodyMaterial
        );
        trunk.position.y = height * 0.5;
        plantGroup.add(trunk);

        const armPairs = [
            { side: -1, heightFactor: 0.5, length: 0.26 + plantDensity * 0.1, tilt: -0.95 },
            { side: 1, heightFactor: 0.66, length: 0.22 + plantDensity * 0.08, tilt: 0.92 }
        ];
        armPairs.forEach((armSpec) => {
            const arm = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.045, 0.055, armSpec.length, 7),
                bodyMaterial
            );
            arm.position.set(armSpec.side * 0.11, height * armSpec.heightFactor, 0);
            arm.rotation.z = armSpec.tilt;
            plantGroup.add(arm);

            const armTip = new THREE_NS.Mesh(
                new THREE_NS.SphereGeometry(0.052, 8, 8),
                bodyMaterial
            );
            armTip.position.set(
                arm.position.x + armSpec.side * (armSpec.length * 0.34),
                arm.position.y + armSpec.length * 0.28,
                0
            );
            armTip.scale.set(1.0, 1.1, 1.0);
            plantGroup.add(armTip);
        });

        const bloom = new THREE_NS.Mesh(
            new THREE_NS.SphereGeometry(0.038, 10, 10),
            createPlantMaterial(0xcfa56a, 0.7, 0x8c5c26, 0.08)
        );
        bloom.position.set(0, height + 0.02, 0);
        bloom.scale.set(1.0, 0.8, 1.0);
        plantGroup.add(bloom);
    }

    function addSunfruitVinePlant(plantGroup, tile, plantDensity) {
        const stemMaterial = createPlantMaterial(0x6e7a37, 0.86);
        const leafColors = [0x8db05e, 0x7a984b, 0x97bc64];
        const fruitColor = 0xd5a34f;
        const span = 0.18 + plantDensity * 0.14;
        const vineCurve = new THREE_NS.CatmullRomCurve3([
            new THREE_NS.Vector3(-span, 0.03, -0.08),
            new THREE_NS.Vector3(-0.06, 0.12 + plantDensity * 0.05, 0.04),
            new THREE_NS.Vector3(0.07, 0.08 + plantDensity * 0.06, -0.03),
            new THREE_NS.Vector3(span, 0.14 + plantDensity * 0.08, 0.08)
        ]);
        const vine = new THREE_NS.Mesh(
            new THREE_NS.TubeGeometry(vineCurve, 16, 0.018, 6, false),
            stemMaterial
        );
        plantGroup.add(vine);

        for (let index = 0; index < 4; index += 1) {
            const t = 0.18 + index * 0.2;
            const point = vineCurve.getPoint(t);
            const tangent = vineCurve.getTangent(t);
            const leaf = createLeafMesh(
                leafColors[index % leafColors.length],
                0.08 + plantDensity * 0.03,
                0.014,
                0.12 + plantDensity * 0.04
            );
            leaf.position.copy(point);
            leaf.position.y += 0.02 + (index % 2) * 0.015;
            leaf.rotation.y = Math.atan2(tangent.x, tangent.z) + (index % 2 === 0 ? 0.6 : -0.65);
            leaf.rotation.z = index % 2 === 0 ? -0.52 : 0.52;
            plantGroup.add(leaf);
        }

        const fruitCount = 1 + Math.round(plantDensity * 2);
        for (let index = 0; index < fruitCount; index += 1) {
            const t = 0.42 + index * 0.18;
            const point = vineCurve.getPoint(Math.min(t, 0.92));
            const fruit = new THREE_NS.Mesh(
                new THREE_NS.SphereGeometry(0.034 + plantDensity * 0.01, 10, 10),
                createPlantMaterial(fruitColor, 0.62, 0x7c4d16, 0.12)
            );
            fruit.position.copy(point);
            fruit.position.y -= 0.015;
            fruit.position.x += (index % 2 === 0 ? -1 : 1) * 0.028;
            plantGroup.add(fruit);
        }
    }

    function addGenericPlant(plantGroup, tile, plantDensity) {
        const stem = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.03, 0.04, 0.26 + plantDensity * 0.18, 6),
            createPlantMaterial(0x72884d)
        );
        stem.position.y = 0.16 + plantDensity * 0.09;
        plantGroup.add(stem);

        for (let index = 0; index < 3; index += 1) {
            const angle = (index / 3) * Math.PI * 2 + deterministicNoise01(tile.x, tile.y, index + 151) * 0.5;
            const leaf = createLeafMesh(0x96b867, 0.05, 0.012, 0.11 + plantDensity * 0.04);
            leaf.position.set(Math.cos(angle) * 0.04, 0.14 + index * 0.04, Math.sin(angle) * 0.04);
            leaf.rotation.y = angle;
            leaf.rotation.z = 0.5;
            plantGroup.add(leaf);
        }
    }

    function createPlantVisual(tile, tileHeight, plantDensity) {
        const plantGroup = new THREE_NS.Group();
        plantGroup.position.y = tileHeight + 0.02;
        plantGroup.rotation.y = deterministicNoise01(tile.x, tile.y, tile.plantTypeId + 161) * Math.PI * 2;

        if (tile.plantTypeId === 1) {
            addWindReedPlant(plantGroup, tile, plantDensity);
        } else if (tile.plantTypeId === 2) {
            addSaltbushPlant(plantGroup, tile, plantDensity);
        } else if (tile.plantTypeId === 3) {
            addShadeCactusPlant(plantGroup, tile, plantDensity);
        } else if (tile.plantTypeId === 4) {
            addSunfruitVinePlant(plantGroup, tile, plantDensity);
        } else {
            addGenericPlant(plantGroup, tile, plantDensity);
        }

        return plantGroup;
    }

    function createStructureVisual(tile, tileHeight) {
        const structureGroup = new THREE_NS.Group();
        structureGroup.position.y = tileHeight;

        if (tile.structureTypeId === 201) {
            const stoveBase = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.52, 0.34, 0.42),
                new THREE_NS.MeshStandardMaterial({ color: 0x7f5740, roughness: 0.88, metalness: 0.1 })
            );
            stoveBase.position.y = 0.18;
            structureGroup.add(stoveBase);

            const cookTop = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.11, 0.11, 0.06, 16),
                new THREE_NS.MeshStandardMaterial({ color: 0x4a3a34, roughness: 0.65, metalness: 0.22 })
            );
            cookTop.position.set(0, 0.38, 0);
            structureGroup.add(cookTop);
        } else if (tile.structureTypeId === 202) {
            const benchTop = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.72, 0.08, 0.46),
                new THREE_NS.MeshStandardMaterial({ color: 0x9e784d, roughness: 0.92, metalness: 0.02 })
            );
            benchTop.position.y = 0.48;
            structureGroup.add(benchTop);

            [-0.28, 0.28].forEach((x) => {
                [-0.16, 0.16].forEach((z) => {
                    const leg = new THREE_NS.Mesh(
                        new THREE_NS.BoxGeometry(0.06, 0.5, 0.06),
                        new THREE_NS.MeshStandardMaterial({ color: 0x6a4a2c, roughness: 0.92, metalness: 0.01 })
                    );
                    leg.position.set(x, 0.22, z);
                    structureGroup.add(leg);
                });
            });
        } else if (tile.structureTypeId === 203) {
            const crate = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.58, 0.46, 0.58),
                new THREE_NS.MeshStandardMaterial({ color: 0x8a633d, roughness: 0.92, metalness: 0.02 })
            );
            crate.position.y = 0.24;
            structureGroup.add(crate);
        } else {
            const fallback = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.5, 0.3, 0.5),
                new THREE_NS.MeshStandardMaterial({ color: 0x8f7a64, roughness: 0.9, metalness: 0.02 })
            );
            fallback.position.y = 0.16;
            structureGroup.add(fallback);
        }

        return structureGroup;
    }

    function rebuildStaticSiteScene(siteBootstrap, offsetX, offsetZ, width, height, previousCache, bootstrapSignature) {
        clearWorld();
        currentSceneKind = "SITE_ACTIVE";
        scene.background = new THREE_NS.Color(0xe7d3b0);
        const cameraOrbitState = buildSiteCameraOrbitState(siteBootstrap, width, height, previousCache);
        const tilePickables = [];

        const desertWidth = Math.max(width + 8, 20);
        const desertHeight = Math.max(height + 8, 20);

        const floor = new THREE_NS.Mesh(
            new THREE_NS.PlaneGeometry(desertWidth, desertHeight, 1, 1),
            new THREE_NS.MeshStandardMaterial({ color: 0xd7bb8b, roughness: 0.98, metalness: 0.01 })
        );
        floor.rotation.x = -Math.PI / 2;
        floor.position.y = -0.05;
        worldGroup.add(floor);

        const siteBorder = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(width + 0.6, 0.08, height + 0.6),
            new THREE_NS.MeshStandardMaterial({ color: 0xbc9b67, roughness: 0.95, metalness: 0.02 })
        );
        siteBorder.position.set(0, -0.01, 0);
        worldGroup.add(siteBorder);

        siteBootstrap.tiles.forEach((tile) => {
            const plantDensity = Math.max(0, Math.min(tile.plantDensity || 0, 1));
            const burial = Math.max(0, Math.min(tile.sandBurial || 0, 1));
            const hasPlant = tile.plantTypeId !== 0 || plantDensity > 0.05;
            const noise = ((tile.x * 17 + tile.y * 29) % 7) * 0.008;
            const tileHeight = 0.08 + burial * 0.14 + plantDensity * 0.18 + noise;
            const tileColor = tile.plantTypeId !== 0
                ? (tile.plantTypeId === 1 ? 0x8ea56a
                    : tile.plantTypeId === 2 ? 0x7f9b62
                    : tile.plantTypeId === 3 ? 0x8a8f58
                    : 0x9d8f5a)
                : hasPlant
                    ? 0x97a66d
                    : burial > 0.45
                        ? 0xdfc090
                        : 0xc9a776;

            const tileMesh = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.94, tileHeight, 0.94),
                new THREE_NS.MeshStandardMaterial({ color: tileColor, roughness: 0.9, metalness: 0.02 })
            );
            tileMesh.position.set(tile.x - offsetX, tileHeight * 0.5, tile.y - offsetZ);
            tileMesh.userData = {
                tileX: tile.x,
                tileY: tile.y
            };
            worldGroup.add(tileMesh);
            tilePickables.push(tileMesh);

            if (hasPlant) {
                const plantVisual = createPlantVisual(tile, tileHeight, plantDensity);
                plantVisual.position.x = tile.x - offsetX;
                plantVisual.position.z = tile.y - offsetZ;
                worldGroup.add(plantVisual);
            }

            if (tile.structureTypeId !== 0) {
                const structureVisual = createStructureVisual(tile, tileHeight);
                structureVisual.position.x = tile.x - offsetX;
                structureVisual.position.z = tile.y - offsetZ;
                worldGroup.add(structureVisual);
            }
        });

        if (siteBootstrap.camp) {
            const campX = siteBootstrap.camp.tileX - offsetX;
            const campZ = siteBootstrap.camp.tileY - offsetZ;

            const campPad = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.56, 0.66, 0.12, 18),
                new THREE_NS.MeshStandardMaterial({ color: 0x866341, roughness: 0.9, metalness: 0.03 })
            );
            campPad.position.set(campX, 0.12, campZ);
            worldGroup.add(campPad);

            const tent = new THREE_NS.Mesh(
                new THREE_NS.ConeGeometry(0.38, 0.6, 4),
                new THREE_NS.MeshStandardMaterial({ color: 0xd8d1bf, roughness: 0.85, metalness: 0.02 })
            );
            tent.position.set(campX, 0.48, campZ);
            tent.rotation.y = Math.PI * 0.25;
            worldGroup.add(tent);
        }

        const workerVisual = createHumanoidWorker();
        const workerGroup = workerVisual.group;
        worldGroup.add(workerGroup);

        siteSceneCache = {
            siteId: siteBootstrap.siteId,
            bootstrapSignature: bootstrapSignature,
            sourceBootstrap: siteBootstrap,
            offsetX: offsetX,
            offsetZ: offsetZ,
            width: width,
            height: height,
            tilePickables: tilePickables,
            workerGroup: workerGroup,
            workerRig: workerVisual.rig,
            workerInitialized: false,
            workerAnimPhase: 0,
            workerActionPhase: 0,
            workerVisualSpeed: 0,
            workerAuthoritativeSpeed: 0,
            workerTargetX: 0,
            workerTargetZ: 0,
            workerTargetYaw: 0,
            workerTargetInitialized: false,
            lastWorkerPatchTimeMs: 0,
            lastWorkerPatchDeltaFrames: 0,
            lastWorkerPatchIntervalMs: 0,
            lastWorkerPatchDistance: 0,
            lastWorkerTargetFrameNumber: 0,
            lastWorkerTargetX: 0,
            lastWorkerTargetZ: 0,
            cameraTargetX: 0,
            cameraTargetZ: 0,
            cameraOrbitGroundDistance: cameraOrbitState.groundDistance,
            cameraOrbitOffsetX: cameraOrbitState.offsetX,
            cameraOrbitOffsetY: cameraOrbitState.offsetY,
            cameraOrbitOffsetZ: cameraOrbitState.offsetZ
        };
    }

    function normalizeAngleRadians(angle) {
        while (angle > Math.PI) {
            angle -= Math.PI * 2;
        }
        while (angle < -Math.PI) {
            angle += Math.PI * 2;
        }
        return angle;
    }

    function blendFactorFromRate(ratePerSecond, deltaSeconds) {
        return 1.0 - Math.exp(-ratePerSecond * deltaSeconds);
    }

    function renderSiteScene(state) {
        scene.background = new THREE_NS.Color(0xe7d3b0);
        const siteBootstrap = getSiteBootstrap(state);
        const siteState = getSiteState(state);

        if (!siteBootstrap) {
            if (currentSceneKind !== "SITE_ACTIVE") {
                clearWorld();
                currentSceneKind = "SITE_ACTIVE";
            }

            if (!siteSceneCache) {
                const placeholder = new THREE_NS.Mesh(
                    new THREE_NS.BoxGeometry(2.4, 1.2, 2.4),
                    new THREE_NS.MeshStandardMaterial({ color: 0x9f7c53, roughness: 0.82 })
                );
                placeholder.position.set(0, 0.6, 0);
                placeholder.userData = { spinY: 0.12 };
                worldGroup.add(placeholder);
            }

            camera.position.set(4.5, 5.8, 7.2);
            camera.lookAt(0, 0.5, 0);
            return;
        }

        const width = Math.max(siteBootstrap.width, 1);
        const height = Math.max(siteBootstrap.height, 1);
        const offsetX = (width - 1) * 0.5;
        const offsetZ = (height - 1) * 0.5;
        const previousSiteSceneCache = siteSceneCache;
        const bootstrapReferenceChanged =
            !siteSceneCache ||
            siteSceneCache.sourceBootstrap !== siteBootstrap;
        const bootstrapSignature = bootstrapReferenceChanged
            ? buildSiteBootstrapSignature(siteBootstrap)
            : siteSceneCache.bootstrapSignature;

        const needsStaticRebuild =
            currentSceneKind !== "SITE_ACTIVE" ||
            !siteSceneCache ||
            siteSceneCache.siteId !== siteBootstrap.siteId ||
            siteSceneCache.width !== width ||
            siteSceneCache.height !== height ||
            (bootstrapReferenceChanged && siteSceneCache.bootstrapSignature !== bootstrapSignature);

        if (needsStaticRebuild) {
            rebuildStaticSiteScene(
                siteBootstrap,
                offsetX,
                offsetZ,
                width,
                height,
                previousSiteSceneCache,
                bootstrapSignature);
        }

        if (!siteSceneCache) {
            return;
        }

        updateSiteWorkerTargetFromState(siteState, state.frameNumber || 0);

        if (needsStaticRebuild) {
            applySiteCameraTransform(
                siteSceneCache,
                siteSceneCache.cameraTargetX,
                siteSceneCache.cameraTargetZ,
                1.0);
        }
    }

    function rebuildWorld(state) {
        switch (state.appState) {
        case "MAIN_MENU":
            clearWorld();
            renderMainMenuScene();
            break;
        case "REGIONAL_MAP":
            clearWorld();
            renderRegionalMapScene(state);
            break;
        case "SITE_ACTIVE":
            renderSiteScene(state);
            break;
        default:
            renderSiteScene(state);
            break;
        }
    }

    function renderState(state) {
        latestState = state;
        updateOverlay(state);
        rebuildWorld(state);
        if (state.appState === "SITE_ACTIVE" && tileContextMenuState) {
            renderTileContextMenu();
        } else if (tileContextMenuState) {
            closeTileContextMenu();
        }
    }

    function buildPresentationSignature(state) {
        return JSON.stringify({
            appState: state.appState,
            selectedSiteId: state.selectedSiteId,
            uiSetups: state.uiSetups,
            regionalMap: state.regionalMap,
            siteResult: state.siteResult
        });
    }

    function animate() {
        requestAnimationFrame(animate);

        const deltaSeconds = Math.min(animationClock.getDelta(), 0.1);
        animationTimeSeconds += deltaSeconds;
        const elapsed = animationTimeSeconds;
        worldGroup.traverse((node) => {
            if (node.userData.spinY) {
                node.rotation.y += node.userData.spinY * 0.01;
            }
            if (node.userData.spinZ) {
                node.rotation.z += node.userData.spinZ * 0.01;
            }
            if (node.userData.bobAmplitude) {
                node.position.y = node.userData.baseY +
                    Math.sin(elapsed + node.userData.bobOffset) * 0.015 * node.userData.bobAmplitude * 10.0;
            }
            if (node.userData.pulse) {
                const pulse = 1.0 + Math.sin((elapsed + node.userData.pulseOffset) * 2.0) * 0.08;
                node.scale.setScalar(pulse);
            }
        });

        if (siteSceneCache && siteSceneCache.workerGroup) {
            const workerGroup = siteSceneCache.workerGroup;
            const rotationBlend = blendFactorFromRate(16.0, deltaSeconds);
            const isWorkerMoving =
                siteSceneCache.workerAuthoritativeSpeed > 0.2 ||
                moveAxes.x !== 0 ||
                moveAxes.y !== 0;
            const cameraFollowRate = isWorkerMoving ? 30.0 : 10.0;
            const cameraBlend =
                cameraOrbitDragPointerId == null ? blendFactorFromRate(cameraFollowRate, deltaSeconds) : 1.0;
            const previousWorkerX = workerGroup.position.x;
            const previousWorkerZ = workerGroup.position.z;
            const targetDeltaX = siteSceneCache.workerTargetX - workerGroup.position.x;
            const targetDeltaZ = siteSceneCache.workerTargetZ - workerGroup.position.z;
            const distanceToTargetBeforeMove = Math.hypot(targetDeltaX, targetDeltaZ);
            if (distanceToTargetBeforeMove > 0.0001 && deltaSeconds > 0.0) {
                const stepDistance = Math.min(
                    distanceToTargetBeforeMove,
                    siteSceneCache.workerAuthoritativeSpeed * deltaSeconds
                );
                if (stepDistance > 0.0) {
                    workerGroup.position.x += (targetDeltaX / distanceToTargetBeforeMove) * stepDistance;
                    workerGroup.position.z += (targetDeltaZ / distanceToTargetBeforeMove) * stepDistance;
                }
            }

            const yawDelta = normalizeAngleRadians(siteSceneCache.workerTargetYaw - workerGroup.rotation.y);
            workerGroup.rotation.y += yawDelta * rotationBlend;

            const visualMoveDistance = Math.hypot(
                workerGroup.position.x - previousWorkerX,
                workerGroup.position.z - previousWorkerZ
            );
            const distanceToTarget = Math.hypot(
                siteSceneCache.workerTargetX - workerGroup.position.x,
                siteSceneCache.workerTargetZ - workerGroup.position.z
            );
            const visualSpeed = deltaSeconds > 0.0 ? visualMoveDistance / deltaSeconds : 0.0;
            const desiredWorkerSpeed = Math.max(siteSceneCache.workerAuthoritativeSpeed, visualSpeed);
            const speedBlend = blendFactorFromRate(12.0, deltaSeconds);
            siteSceneCache.workerVisualSpeed += (desiredWorkerSpeed - siteSceneCache.workerVisualSpeed) * speedBlend;
            const currentActionKind =
                latestState && latestState.siteState && latestState.siteState.worker
                    ? latestState.siteState.worker.currentActionKind || 0
                    : 0;
            updateHumanoidWorkerAnimation(
                siteSceneCache,
                deltaSeconds,
                elapsed,
                siteSceneCache.workerVisualSpeed,
                distanceToTarget,
                currentActionKind
            );

            applySiteCameraTransform(
                siteSceneCache,
                workerGroup.position.x,
                workerGroup.position.z,
                cameraBlend);
        }

        fitRenderer();
        renderActionProgressBar(latestState);
        renderer.render(scene, camera);
    }

    function normalizeState(state) {
        if (!state.uiSetups) {
            state.uiSetups = [];
        }
        if (!state.regionalMap) {
            state.regionalMap = { sites: [], links: [] };
        }
        if (typeof state.siteAction === "undefined") {
            state.siteAction = null;
        }
        return state;
    }

    function mergeStatePatch(patch) {
        const baseState = normalizeState(Object.assign({}, latestState || {}));
        const mergedState = Object.assign({}, baseState, patch);
        if (patch.siteStatePatch) {
            mergedState.siteState = Object.assign({}, baseState.siteState || {}, patch.siteStatePatch);
            delete mergedState.siteStatePatch;
        }
        return normalizeState(mergedState);
    }

    function getSiteLightweightPatchParts(patch) {
        if (!patch) {
            return null;
        }

        const patchFields = Object.keys(patch).filter((key) => key !== "frameNumber");
        if (patchFields.length === 0) {
            return null;
        }

        const lightweightParts = {
            worker: false,
            weather: false,
            hud: false,
            siteAction: false
        };

        for (const field of patchFields) {
            if (field === "hud") {
                lightweightParts.hud = true;
                continue;
            }
            if (field === "siteAction") {
                lightweightParts.siteAction = true;
                continue;
            }

            if (field !== "siteStatePatch" || !patch.siteStatePatch) {
                return null;
            }

            const siteStateFields = Object.keys(patch.siteStatePatch);
            for (const siteField of siteStateFields) {
                if (siteField === "worker") {
                    lightweightParts.worker = true;
                    continue;
                }
                if (siteField === "weather") {
                    lightweightParts.weather = true;
                    continue;
                }
                return null;
            }
        }

        if (!lightweightParts.worker && !lightweightParts.weather && !lightweightParts.hud && !lightweightParts.siteAction) {
            return null;
        }

        return lightweightParts;
    }

    function handleIncomingState(state, forceRender, patch) {
        const normalizedState = normalizeState(state);
        viewerCompatibilityWarning = getVisualSmokeCompatibilityWarning(normalizedState);
        rebuildInventoryCache(normalizedState);
        const previousState = latestState;
        syncLocalActionProgress(normalizedState);
        const lightweightPatchParts =
            !forceRender && normalizedState.appState === "SITE_ACTIVE"
                ? getSiteLightweightPatchParts(patch)
                : null;
        if (lightweightPatchParts) {
            latestState = normalizedState;
            if (lightweightPatchParts.worker) {
                updateSiteWorkerTargetFromState(getSiteState(normalizedState), normalizedState.frameNumber || 0);
            }

            const previousActionKind =
                previousState && previousState.siteState && previousState.siteState.worker
                    ? previousState.siteState.worker.currentActionKind || 0
                    : 0;
            const nextActionKind =
                normalizedState.siteState && normalizedState.siteState.worker
                    ? normalizedState.siteState.worker.currentActionKind || 0
                    : 0;
            if (lightweightPatchParts.hud ||
                lightweightPatchParts.siteAction ||
                lightweightPatchParts.weather ||
                previousActionKind !== nextActionKind) {
                renderSiteStatusChip(normalizedState);
            }
            if (lightweightPatchParts.hud) {
                renderSiteInventoryPanel(
                    normalizedState,
                    getInventorySlotsByKind(normalizedState, "WORKER_PACK"));
                renderStoragePanel(normalizedState, findOpenedInventoryContainer(normalizedState));
                if (phonePanelOpen) {
                    renderPhonePanel(normalizedState);
                }
            }
            renderActionProgressBar(normalizedState);
            return;
        }

        if (forceRender || normalizedState.appState === "SITE_ACTIVE") {
            if (normalizedState.appState === "SITE_ACTIVE") {
                latestPresentationSignature = "";
            } else {
                latestPresentationSignature = buildPresentationSignature(normalizedState);
            }
            renderState(normalizedState);
        } else {
            const presentationSignature = buildPresentationSignature(normalizedState);
            if (presentationSignature !== latestPresentationSignature) {
                latestPresentationSignature = presentationSignature;
                renderState(normalizedState);
            } else {
                latestState = normalizedState;
            }
        }

        if (normalizedState.appState !== "MAIN_MENU" && normalizedState.appState !== "SITE_ACTIVE") {
            statusChip.textContent =
                "Connected\nFrame " + normalizedState.frameNumber +
                "\nSelected Site: " + (normalizedState.selectedSiteId == null ? "none" : normalizedState.selectedSiteId);
        }
        renderActionProgressBar(normalizedState);
    }

    function connectStateStream() {
        if (stateStream) {
            stateStream.close();
        }

        stateStream = new EventSource("/events");
        stateStream.addEventListener("full-state", function (event) {
            const state = normalizeState(JSON.parse(event.data));
            handleIncomingState(state, true);
        });
        stateStream.addEventListener("state-patch", function (event) {
            const patch = JSON.parse(event.data);
            const state = mergeStatePatch(patch);
            handleIncomingState(state, false, patch);
        });
        stateStream.onerror = function () {
            statusChip.textContent = "Waiting for host...";
        };
    }

    function computeSiteControlState() {
        if (!latestState || latestState.appState !== "SITE_ACTIVE") {
            return {
                hasMoveInput: 0,
                worldMoveX: 0,
                worldMoveY: 0,
                worldMoveZ: 0
            };
        }

        if (moveAxes.x === 0 && moveAxes.y === 0) {
            return {
                hasMoveInput: 0,
                worldMoveX: 0,
                worldMoveY: 0,
                worldMoveZ: 0
            };
        }

        camera.getWorldDirection(cameraForwardOnGround);
        cameraForwardOnGround.y = 0;
        if (cameraForwardOnGround.lengthSq() <= 0.000001) {
            return {
                hasMoveInput: 0,
                worldMoveX: 0,
                worldMoveY: 0,
                worldMoveZ: 0
            };
        }

        cameraForwardOnGround.normalize();
        cameraRightOnGround.crossVectors(cameraForwardOnGround, worldUp);
        if (cameraRightOnGround.lengthSq() <= 0.000001) {
            return {
                hasMoveInput: 0,
                worldMoveX: 0,
                worldMoveY: 0,
                worldMoveZ: 0
            };
        }

        cameraRightOnGround.normalize();
        desiredMoveOnGround.copy(cameraForwardOnGround).multiplyScalar(moveAxes.y);
        desiredMoveOnGround.addScaledVector(cameraRightOnGround, moveAxes.x);
        desiredMoveOnGround.y = 0;
        if (desiredMoveOnGround.lengthSq() <= 0.000001) {
            return {
                hasMoveInput: 0,
                worldMoveX: 0,
                worldMoveY: 0,
                worldMoveZ: 0
            };
        }

        desiredMoveOnGround.normalize();
        return {
            hasMoveInput: 1,
            worldMoveX: desiredMoveOnGround.x,
            worldMoveY: desiredMoveOnGround.z,
            worldMoveZ: 0
        };
    }

    function sendSiteControlState() {
        if (siteControlSendInFlight) {
            return;
        }

        const siteControlState = computeSiteControlState();
        const signature = siteControlState.hasMoveInput !== 0
            ? [
                "1",
                siteControlState.worldMoveX.toFixed(6),
                siteControlState.worldMoveY.toFixed(6),
                siteControlState.worldMoveZ.toFixed(6)
            ].join(":")
            : "0";
        if (signature === lastSentSiteControlSignature) {
            return;
        }

        siteControlSendInFlight = true;
        postJson("/site-control", siteControlState)
            .then(() => {
                lastSentSiteControlSignature = signature;
            })
            .catch(() => {})
            .finally(() => {
                siteControlSendInFlight = false;
            });
    }

    window.addEventListener("keydown", function (event) {
        if (event.code === "KeyF" && !event.repeat && latestState && latestState.appState === "SITE_ACTIVE") {
            phonePanelOpen = !phonePanelOpen;
            renderPhonePanel(latestState);
            event.preventDefault();
            return;
        }
        if (event.code === "KeyB" && !event.repeat && latestState && latestState.appState === "SITE_ACTIVE") {
            inventoryPanelOpen = !inventoryPanelOpen;
            renderSiteOverlay(latestState);
            event.preventDefault();
            return;
        }
        if (event.code === "Escape" && tileContextMenuState) {
            closeTileContextMenu();
        }
        keys.add(event.code);
        updateMoveAxis();
        sendSiteControlState();
    });

    window.addEventListener("keyup", function (event) {
        keys.delete(event.code);
        updateMoveAxis();
        sendSiteControlState();
    });

    window.addEventListener("contextmenu", function (event) {
        if (!shouldSuppressSiteContextMenu()) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();
    }, true);

    renderer.domElement.addEventListener("contextmenu", function (event) {
        if (!isSiteActiveView()) {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
    }, true);

    renderer.domElement.addEventListener("mousedown", function (event) {
        if (!isSiteOrbitMouseEvent(event)) {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
    }, true);

    renderer.domElement.addEventListener("mousemove", function (event) {
        if (!isSiteOrbitMouseEvent(event) && !isSiteCameraOrbitActive()) {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
    }, true);

    renderer.domElement.addEventListener("mouseup", function (event) {
        if (!isSiteOrbitMouseEvent(event) && !(isSiteCameraOrbitActive() && event.button === cameraOrbitDragButton)) {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
    }, true);

    renderer.domElement.addEventListener("dragstart", function (event) {
        if (!isSiteActiveView()) {
            return;
        }
        event.preventDefault();
    }, true);

    renderer.domElement.addEventListener("selectstart", function (event) {
        if (!isSiteActiveView()) {
            return;
        }
        event.preventDefault();
    }, true);

    renderer.domElement.addEventListener("auxclick", function (event) {
        if (!isSiteOrbitMouseEvent(event) && event.button !== 1) {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
    }, true);

    renderer.domElement.addEventListener("wheel", function (event) {
        if (!isSiteActiveView() || !siteSceneCache) {
            return;
        }

        updateSiteCameraZoomFromWheel(event.deltaY || 0);
        event.preventDefault();
        event.stopPropagation();
    }, { passive: false });

    renderer.domElement.addEventListener("pointerdown", function (event) {
        const orbitDragButton = getSiteOrbitDragButtonFromEvent(event);
        if (orbitDragButton < 0 || !siteSceneCache) {
            return;
        }

        beginPendingSiteOrbitGesture(event);
        event.preventDefault();
        event.stopPropagation();
    });

    renderer.domElement.addEventListener("pointermove", function (event) {
        if (pendingSiteOrbitGesture && event.pointerId === pendingSiteOrbitGesture.pointerId) {
            const dragDistance = Math.hypot(
                event.clientX - pendingSiteOrbitGesture.startClientX,
                event.clientY - pendingSiteOrbitGesture.startClientY
            );
            if (!pendingSiteOrbitGesture.dragging && dragDistance >= orbitDragThresholdPixels && siteSceneCache) {
                pendingSiteOrbitGesture.dragging = true;
                beginSiteCameraOrbitDrag(event.pointerId, event.clientX);
            }
        }

        if (cameraOrbitDragPointerId == null || event.pointerId !== cameraOrbitDragPointerId || !siteSceneCache) {
            return;
        }
        if (cameraOrbitPointerLockActive) {
            event.preventDefault();
            return;
        }

        const deltaX = event.clientX - cameraOrbitDragLastClientX;
        cameraOrbitDragLastClientX = event.clientX;
        updateCameraOrbitFromDelta(deltaX);
        event.preventDefault();
    });

    renderer.domElement.addEventListener("pointerup", function (event) {
        if (!pendingSiteOrbitGesture || event.pointerId !== pendingSiteOrbitGesture.pointerId) {
            return;
        }

        const wasDragging = pendingSiteOrbitGesture.dragging;
        const releaseClientX = event.clientX;
        const releaseClientY = event.clientY;
        releaseCameraPointerLock();
        stopCameraOrbitDrag(event.pointerId);
        clearPendingSiteOrbitGesture(event.pointerId);
        if (!wasDragging && isSiteActiveView()) {
            const tileData = pickSiteTileAtClientPosition(releaseClientX, releaseClientY);
            if (!tileData) {
                closeTileContextMenu();
            } else {
                openTileContextMenu(tileData, releaseClientX, releaseClientY);
            }
        }
        event.preventDefault();
        event.stopPropagation();
    });

    renderer.domElement.addEventListener("pointercancel", function (event) {
        if (!pendingSiteOrbitGesture || event.pointerId !== pendingSiteOrbitGesture.pointerId) {
            return;
        }
        releaseCameraPointerLock();
        stopCameraOrbitDrag(event.pointerId);
        clearPendingSiteOrbitGesture(event.pointerId);
    });

    renderer.domElement.addEventListener("lostpointercapture", function (event) {
        if (pendingSiteOrbitGesture && event.pointerId === pendingSiteOrbitGesture.pointerId) {
            pendingSiteOrbitGesture = null;
        }
        if (event.pointerId !== cameraOrbitDragPointerId) {
            return;
        }
        cameraOrbitDragPointerId = null;
        cameraOrbitDragButton = -1;
        cameraOrbitDragLastClientX = 0;
    });

    document.addEventListener("pointerlockchange", function () {
        cameraOrbitPointerLockActive = isSiteCameraPointerLocked();
        if (!cameraOrbitPointerLockActive && isSiteCameraOrbitActive()) {
            cameraOrbitDragLastClientX = 0;
        }
    });

    document.addEventListener("mousemove", function (event) {
        if (!cameraOrbitPointerLockActive || !isSiteCameraOrbitActive()) {
            return;
        }

        updateCameraOrbitFromDelta(event.movementX || 0);
        event.preventDefault();
    }, true);

    document.addEventListener("mouseup", function (event) {
        if (!isSiteCameraOrbitActive() && (!pendingSiteOrbitGesture || event.button !== orbitMouseButton)) {
            return;
        }

        releaseCameraPointerLock();
        stopCameraOrbitDrag();
        clearPendingSiteOrbitGesture();
        event.preventDefault();
    }, true);

    window.addEventListener("blur", function () {
        releaseCameraPointerLock();
        stopCameraOrbitDrag();
        clearPendingSiteOrbitGesture();
    });

    document.addEventListener("pointerdown", function (event) {
        if (!tileContextMenuState) {
            return;
        }
        if (tileContextMenu.contains(event.target)) {
            return;
        }
        closeTileContextMenu();
    }, true);

    renderer.domElement.addEventListener("click", function (event) {
        if (!latestState || latestState.appState !== "REGIONAL_MAP") {
            return;
        }

        const rect = renderer.domElement.getBoundingClientRect();
        pointer.x = ((event.clientX - rect.left) / Math.max(rect.width, 1)) * 2 - 1;
        pointer.y = -(((event.clientY - rect.top) / Math.max(rect.height, 1)) * 2 - 1);
        raycaster.setFromCamera(pointer, camera);

        const hits = raycaster.intersectObjects(mapPickables, false);
        if (hits.length > 0) {
            postJson("/ui-action", {
                type: "SELECT_DEPLOYMENT_SITE",
                targetId: hits[0].object.userData.siteId,
                arg0: 0,
                arg1: 0
            }).catch(() => {
                statusChip.textContent = "Failed to select site.";
            });
            return;
        }

        const selectionSetup = getSetup(latestState, "REGIONAL_MAP_SELECTION");
        const backgroundElement = getBackgroundClickElement(selectionSetup);
        if (!backgroundElement) {
            return;
        }

        postJson("/ui-action", backgroundElement.action).catch(() => {
            statusChip.textContent = "Failed to send regional map background click.";
        });
    });

    window.addEventListener("resize", fitRenderer);

    fitRenderer();
    storagePanelClose.addEventListener("click", function () {
        closeOpenedInventoryContainer();
    });
    animate();
    connectStateStream();
    window.setInterval(sendSiteControlState, 16);
})();
