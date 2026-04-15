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
    const contextActions = document.getElementById("context-actions");
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
    let tileContextMenuState = null;
    let localActionProgressState = null;
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
    const inventoryContainerCodes = {
        WORKER_PACK: 0,
        CAMP_STORAGE: 1
    };
    const itemCatalog = {
        1: { name: "Water", shortName: "H2O", stackSize: 5, canUse: true, canPlant: false },
        2: { name: "Food", shortName: "Ration", stackSize: 5, canUse: true, canPlant: false },
        3: { name: "Medicine", shortName: "Med", stackSize: 3, canUse: true, canPlant: false },
        4: { name: "Wind Reed Seeds", shortName: "Wind Reed", stackSize: 10, canUse: false, canPlant: true },
        5: { name: "Saltbush Seeds", shortName: "Saltbush", stackSize: 10, canUse: false, canPlant: true },
        6: { name: "Shade Cactus Seeds", shortName: "Shade Cactus", stackSize: 10, canUse: false, canPlant: true },
        7: { name: "Sunfruit Vine Seeds", shortName: "Sunfruit Vine", stackSize: 10, canUse: false, canPlant: true }
    };
    const itemVisuals = {
        1: { glyph: "H2", light: "#5d8eb3", dark: "#33546f" },
        2: { glyph: "FD", light: "#b48a4f", dark: "#6f4a28" },
        3: { glyph: "MD", light: "#9a676c", dark: "#62363b" },
        4: { glyph: "WR", light: "#79a35f", dark: "#46693a" },
        5: { glyph: "SB", light: "#8aa05c", dark: "#53683a" },
        6: { glyph: "SC", light: "#6a8c67", dark: "#3c5841" },
        7: { glyph: "SV", light: "#8f7c4c", dark: "#5c4526" }
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

    function getInventorySlotsByKind(state, containerKind) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.inventorySlots) {
            return [];
        }

        return siteState.inventorySlots.filter((slot) => slot.containerKind === containerKind);
    }

    function getBuyListings(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.phoneListings) {
            return [];
        }

        return siteState.phoneListings.filter((listing) => listing.listingKind === "BUY_ITEM" && listing.quantity !== 0);
    }

    function slotKey(containerKind, slotIndex) {
        return containerKind + ":" + slotIndex;
    }

    function encodeInventoryUseArg(containerKind, slotIndex, quantity) {
        const containerCode = inventoryContainerCodes[containerKind] || 0;
        return containerCode + ((slotIndex & 0xff) << 8) + ((quantity & 0xffff) << 16);
    }

    function encodeInventoryTransferArg(
        sourceContainerKind,
        sourceSlotIndex,
        destinationContainerKind,
        destinationSlotIndex,
        quantity
    ) {
        const sourceContainerCode = inventoryContainerCodes[sourceContainerKind] || 0;
        const destinationContainerCode = inventoryContainerCodes[destinationContainerKind] || 0;
        return sourceContainerCode +
            ((sourceSlotIndex & 0xff) << 8) +
            ((destinationContainerCode & 0xff) << 16) +
            ((destinationSlotIndex & 0xff) << 24) +
            ((quantity & 0xffff) * 0x100000000);
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

    function clearSelectedInventorySlotIfInvalid(state) {
        if (!selectedInventorySlotKey || !state || state.appState !== "SITE_ACTIVE") {
            selectedInventorySlotKey = "";
            return;
        }

        const allSlots = getInventorySlotsByKind(state, "WORKER_PACK").concat(getInventorySlotsByKind(state, "CAMP_STORAGE"));
        const selectedSlot = allSlots.find((slot) => slotKey(slot.containerKind, slot.slotIndex) === selectedInventorySlotKey);
        if (!selectedSlot || selectedSlot.quantity <= 0 || selectedSlot.flags === 0) {
            selectedInventorySlotKey = "";
        }
    }

    function findSelectedInventorySlot(state) {
        if (!selectedInventorySlotKey || !state) {
            return null;
        }
        const allSlots = getInventorySlotsByKind(state, "WORKER_PACK").concat(getInventorySlotsByKind(state, "CAMP_STORAGE"));
        return allSlots.find((slot) => slotKey(slot.containerKind, slot.slotIndex) === selectedInventorySlotKey) || null;
    }

    function findTransferTargetSlot(state, sourceSlot, destinationKind) {
        const destinationSlots = getInventorySlotsByKind(state, destinationKind);
        const itemMeta = getItemMeta(sourceSlot.itemId);
        const stackSize = itemMeta ? itemMeta.stackSize : Math.max(sourceSlot.quantity, 1);

        for (const destinationSlot of destinationSlots) {
            if (destinationSlot.flags === 0 && destinationSlot.itemId === 0) {
                return {
                    slotIndex: destinationSlot.slotIndex,
                    quantity: Math.min(sourceSlot.quantity, stackSize)
                };
            }
            if (destinationSlot.flags === 0) {
                return {
                    slotIndex: destinationSlot.slotIndex,
                    quantity: Math.min(sourceSlot.quantity, stackSize)
                };
            }
            if (destinationSlot.itemId !== sourceSlot.itemId) {
                continue;
            }

            const freeCapacity = stackSize - destinationSlot.quantity;
            if (freeCapacity <= 0) {
                continue;
            }

            return {
                slotIndex: destinationSlot.slotIndex,
                quantity: Math.min(sourceSlot.quantity, freeCapacity)
            };
        }

        return null;
    }

    function postInventoryUse(slot) {
        return postJson("/ui-action", {
            type: "USE_INVENTORY_ITEM",
            targetId: slot.itemId,
            arg0: encodeInventoryUseArg(slot.containerKind, slot.slotIndex, 1),
            arg1: 0
        });
    }

    function postInventoryTransfer(sourceSlot, destinationKind, destinationSlotIndex, quantity) {
        return postJson("/ui-action", {
            type: "TRANSFER_INVENTORY_ITEM",
            targetId: 0,
            arg0: encodeInventoryTransferArg(
                sourceSlot.containerKind,
                sourceSlot.slotIndex,
                destinationKind,
                destinationSlotIndex,
                quantity
            ),
            arg1: 0
        });
    }

    function postSiteAction(payload) {
        return postJson("/site-action", payload);
    }

    function getSiteActionLabel(actionKind) {
        if (actionKind === 1) {
            return "Plant";
        }
        if (actionKind === 4) {
            return "Water";
        }
        if (actionKind === 5) {
            return "Clear Burial";
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
            "Tile " + localActionProgressState.targetTileX + ", " + localActionProgressState.targetTileY;
    }

    function renderSiteStatusChip(state) {
        const siteState = getSiteState(state);
        const hud = state ? state.hud : null;
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
        const carriedSeeds = getCarriedSeedOptions(state);
        if (carriedSeeds.length === 0) {
            return [
                {
                    id: "none",
                    label: "No Action",
                    meta: "Carry seeds in the worker pack to plant here.",
                    iconGlyph: "--",
                    iconLight: "#a38e76",
                    iconDark: "#796652",
                    disabled: true
                }
            ];
        }

        return [
            {
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
            }
        ];
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
        const locationName = slot.containerKind === "WORKER_PACK" ? "Worker Pack" : "Camp Storage";
        const actionHint = slot.containerKind === "WORKER_PACK"
                ? (itemMeta && itemMeta.canPlant ? "Click to select and arm/store" : "Click to select and use/store")
                : "Click to select and carry";

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
        if (occupied && options.armedSlotKey && slotKey(slot.containerKind, slot.slotIndex) === options.armedSlotKey) {
            slotClasses.push("armed");
        }
        if (occupied && options.selectedSlotKey && slotKey(slot.containerKind, slot.slotIndex) === options.selectedSlotKey) {
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

        if (occupied) {
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

        if (occupied) {
            card.addEventListener("click", function () {
                const clickedKey = slotKey(slot.containerKind, slot.slotIndex);
                selectedInventorySlotKey = selectedInventorySlotKey === clickedKey ? "" : clickedKey;
                if (latestState) {
                    renderSiteOverlay(latestState);
                }
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

    function renderSiteInventoryPanel(state, workerPackSlots, campStorageSlots) {
        selectionInventory.hidden = false;
        selectionInventory.innerHTML = "";

        const stack = document.createElement("div");
        stack.className = "site-panel-stack";
        selectionInventory.appendChild(stack);

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

        appendInventoryGridSection(topGrid, "Camp Storage", "Live camp stock", campStorageSlots, {
            state: state,
            containerKind: "CAMP_STORAGE",
            slotCount: 24,
            columns: 6,
            slotLabelPrefix: "Camp",
            selectedSlotKey: selectedInventorySlotKey
        });

        const footnote = document.createElement("div");
        footnote.className = "inventory-footnote";
        const carriedSeeds = getCarriedSeedOptions(state);
        footnote.textContent = carriedSeeds.length > 0
            ? ("Carry seeds in the worker pack, then right-click a tile to open the planting menu. " +
                carriedSeeds.map((seed) => seed.shortLabel + " x" + seed.quantity).join("  |  "))
            : "Worker pack and camp storage are live and interactive. Move seeds into the worker pack to plant.";
        stack.appendChild(footnote);
    }

    function appendSelectedInventoryActions(state, selectedSlot) {
        if (!selectedSlot) {
            const helper = document.createElement("div");
            helper.className = "helper-note";
            helper.textContent = "Hover any item for details. Click a worker-pack or camp-storage slot to surface actions here.";
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
            (selectedSlot.containerKind === "WORKER_PACK" ? "Worker Pack" : "Camp Storage") +
            " slot " + (selectedSlot.slotIndex + 1);
        contextActions.appendChild(slotSummary);

        if (selectedSlot.containerKind === "WORKER_PACK" && itemMeta && itemMeta.canUse) {
            contextActions.appendChild(
                makeButton(
                    "Use " + getInventoryItemLabel(selectedSlot),
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

        if (selectedSlot.containerKind === "WORKER_PACK") {
            const campTarget = findTransferTargetSlot(state, selectedSlot, "CAMP_STORAGE");
            contextActions.appendChild(
                makeButton(
                    "Store To Camp",
                    function () {
                        if (!campTarget) {
                            return;
                        }
                        postInventoryTransfer(
                            selectedSlot,
                            "CAMP_STORAGE",
                            campTarget.slotIndex,
                            campTarget.quantity).catch(() => {
                            statusChip.textContent = "Failed to store inventory item.";
                        });
                    },
                    true,
                    !campTarget
                )
            );
        }

        if (selectedSlot.containerKind === "CAMP_STORAGE") {
            const workerTarget = findTransferTargetSlot(state, selectedSlot, "WORKER_PACK");
            contextActions.appendChild(
                makeButton(
                    "Carry To Pack",
                    function () {
                        if (!workerTarget) {
                            return;
                        }
                        postInventoryTransfer(
                            selectedSlot,
                            "WORKER_PACK",
                            workerTarget.slotIndex,
                            workerTarget.quantity).catch(() => {
                            statusChip.textContent = "Failed to carry inventory item.";
                        });
                    },
                    true,
                    !workerTarget
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
        contextActions.innerHTML = "";
    }

    function renderRegionalMapOverlay(state) {
        const selectionSetup = getSetup(state, "REGIONAL_MAP_SELECTION");
        const labels = getLabelElements(selectionSetup);
        const actions = getVisibleActionElements(selectionSetup);

        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Field Survey";

        if (labels.length > 0) {
            selectionText.textContent = labels.map((entry) => entry.text).join(" | ");
        } else if (state.selectedSiteId != null) {
            selectionText.textContent = "Deployment route prepared for Site " + state.selectedSiteId + ". Review the dossier and commit when ready.";
        } else {
            selectionText.textContent = "Review the campaign survey and choose the next exposed site to stabilize.";
        }

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
        contextActions.innerHTML = "";
    }

    function renderSiteOverlay(state) {
        const siteBootstrap = getSiteBootstrap(state);
        const siteState = getSiteState(state);
        const workerPackSlots = getInventorySlotsByKind(state, "WORKER_PACK");
        const campStorageSlots = getInventorySlotsByKind(state, "CAMP_STORAGE");
        const buyListings = getBuyListings(state);
        const carriedSeeds = getCarriedSeedOptions(state);
        const workerActionKind =
            siteState && siteState.worker ? siteState.worker.currentActionKind : 0;

        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Site Active";
        contextActions.innerHTML = "";
        clearSelectedInventorySlotIfInvalid(state);

        const selectedSlot = findSelectedInventorySlot(state);
        const plantingText = carriedSeeds.length > 0
            ? ("Right-click any site tile to open the planting menu. Carried seeds: " +
                carriedSeeds.map((seed) => seed.shortLabel + " x" + seed.quantity).join("  |  "))
            : "Carry seeds into the worker pack to unlock planting from the tile menu.";
        selectionText.innerHTML = siteBootstrap
            ? (
                "Site " + siteBootstrap.siteId +
                " is live. Use WASD to move, drag with right mouse to orbit the camera, and short right-click a tile to choose a site action." +
                "<br><br>" + plantingText
            )
            : "Site bootstrap is loading.";
        renderSiteInventoryPanel(state, workerPackSlots, campStorageSlots);
        appendSelectedInventoryActions(state, selectedSlot);

        buyListings.forEach((listing) => {
            contextActions.appendChild(
                makeButton(
                    "Buy " + getItemLabel(listing.itemOrUnlockableId) + " $" + listing.price,
                    function () {
                        postJson("/ui-action", {
                            type: "BUY_PHONE_LISTING",
                            targetId: listing.listingId,
                            arg0: 1,
                            arg1: 0
                        }).catch(() => {
                            statusChip.textContent = "Failed to buy listing.";
                        });
                    },
                    false,
                    false
                )
            );
        });
        renderSiteStatusChip(state);
    }

    function updateOverlay(state) {
        stageFrame.classList.toggle("main-menu-mode", state.appState === "MAIN_MENU");
        stageFrame.classList.toggle("regional-map-mode", state.appState === "REGIONAL_MAP");
        stageFrame.classList.toggle("site-active-mode", state.appState === "SITE_ACTIVE");
        hudEyebrow.textContent = "App State";
        hudTitle.textContent = state.appState || "NONE";

        switch (state.appState) {
        case "MAIN_MENU":
            hudSubtitle.textContent = "Austere, painterly, and severe: the campaign opens under hostile conditions.";
            selectedInventorySlotKey = "";
            clearSelectionInventory();
            renderMenuOverlay(state);
            statusChip.textContent = "Prototype Build\nVisual Smoke";
            break;
        case "REGIONAL_MAP":
            hudSubtitle.textContent = "Review the campaign survey board and choose the next deployment route.";
            selectedInventorySlotKey = "";
            clearSelectionInventory();
            renderRegionalMapOverlay(state);
            statusChip.textContent =
                "Campaign Survey\nFrame " + state.frameNumber +
                "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            break;
        case "SITE_ACTIVE":
            hudSubtitle.textContent = "Field movement is now live. Drag with right mouse to orbit the follow camera, or short right-click a tile to open its action menu.";
            renderSiteOverlay(state);
            break;
        default:
            hudSubtitle.textContent = "The current adapter only styles the core early flow for now.";
            selectedInventorySlotKey = "";
            clearSelectionInventory();
            renderFallbackOverlay(state);
            statusChip.textContent =
                "Connected\nFrame " + state.frameNumber +
                "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            break;
        }
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

        const plantingBlend = currentActionKind === 1 ? clamp01(1.0 - locomotionAmount * 1.8) : 0.0;
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
                const plantHeight = tile.plantTypeId === 4
                    ? 0.22 + plantDensity * 0.35
                    : 0.25 + plantDensity * 0.45;
                const plantGeometry = tile.plantTypeId === 3
                    ? new THREE_NS.CylinderGeometry(0.08, 0.12, plantHeight, 7)
                    : new THREE_NS.CylinderGeometry(0.04, 0.08, plantHeight, tile.plantTypeId === 4 ? 5 : 6);
                const plantMaterial = new THREE_NS.MeshStandardMaterial({
                    color: tile.plantTypeId === 1 ? 0x657c4f
                        : tile.plantTypeId === 2 ? 0x6b8452
                        : tile.plantTypeId === 3 ? 0x7b9150
                        : 0x8c7b4d,
                    roughness: 0.82,
                    metalness: 0.01
                });
                const reed = new THREE_NS.Mesh(
                    plantGeometry,
                    plantMaterial
                );
                reed.position.set(tile.x - offsetX, tileHeight + 0.16 + plantDensity * 0.12, tile.y - offsetZ);
                worldGroup.add(reed);
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
    animate();
    connectStateStream();
    window.setInterval(sendSiteControlState, 16);
})();
