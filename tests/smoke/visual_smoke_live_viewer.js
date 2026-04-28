import * as THREE_NS from "https://unpkg.com/three@0.165.0/build/three.module.js";

(function () {
    const stageFrame = document.getElementById("stage-frame");
    const gameView = document.getElementById("game-view");
    const windOverlay = document.getElementById("wind-overlay");
    const hudEyebrow = document.getElementById("hud-eyebrow");
    const hudTitle = document.getElementById("hud-title");
    const hudSubtitle = document.getElementById("hud-subtitle");
    const siteVitalsPanel = document.getElementById("site-vitals-panel");
    const siteVitalsMoney = document.getElementById("site-vitals-money");
    const siteVitalsReputation = document.getElementById("site-vitals-reputation");
    const siteVitalsBars = document.getElementById("site-vitals-bars");
    const fpsChip = document.getElementById("fps-chip");
    const statusChip = document.getElementById("status-chip");
    const menuPanel = document.getElementById("menu-panel");
    const menuEyebrow = document.getElementById("menu-eyebrow");
    const menuTitle = document.getElementById("menu-title");
    const menuSubtitle = document.getElementById("menu-subtitle");
    const menuCopy = document.getElementById("menu-copy");
    const menuMeta = document.getElementById("menu-meta");
    const menuActions = document.getElementById("menu-actions");
    const selectionChip = document.getElementById("selection-chip");
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
    const phoneAppTitle = document.getElementById("phone-app-title");
    const phoneAppSubtitle = document.getElementById("phone-app-subtitle");
    const phoneScreenBody = document.getElementById("phone-screen-body");
    const actionProgress = document.getElementById("action-progress");
    const actionProgressLabel = document.getElementById("action-progress-label");
    const actionProgressTime = document.getElementById("action-progress-time");
    const actionProgressTitle = document.getElementById("action-progress-title");
    const actionProgressFill = document.getElementById("action-progress-fill");
    const actionProgressPercent = document.getElementById("action-progress-percent");
    const actionProgressTarget = document.getElementById("action-progress-target");
    const placementToast = document.getElementById("placement-toast");
    const placementToastBody = document.getElementById("placement-toast-body");
    const inventoryTooltip = document.getElementById("inventory-tooltip");
    const inventoryTooltipTitle = document.getElementById("inventory-tooltip-title");
    const inventoryTooltipMeta = document.getElementById("inventory-tooltip-meta");
    const modifierTooltip = document.getElementById("modifier-tooltip");
    const modifierTooltipTitle = document.getElementById("modifier-tooltip-title");
    const modifierTooltipMeta = document.getElementById("modifier-tooltip-meta");
    const techTooltip = document.getElementById("tech-tooltip");
    const techTooltipTitle = document.getElementById("tech-tooltip-title");
    const techTooltipMeta = document.getElementById("tech-tooltip-meta");
    const buffModifierStrip = document.getElementById("buff-modifier-strip");
    const normalModifierStrip = document.getElementById("normal-modifier-strip");
    const tileContextMenu = document.getElementById("tile-context-menu");
    const audioToggle = document.getElementById("audio-toggle");

    let latestState = null;
    let technologyCatalog = null;
    let technologyCatalogPromise = null;
    let stateStream = null;
    let mapPickables = [];
    let latestPresentationSignature = "";
    let currentSceneKind = "";
    let siteSceneCache = null;
    let selectedInventorySlotKey = "";
    let openedInventoryContainerKey = "";
    let inventoryPanelOpen = true;
    let phonePanelOpen = false;
    let activeTechTreePanelTabId = "unlockables";
    let phoneBodyScrollTop = 0;
    const phoneSectionScrollTops = {};
    let phonePointerInteractionActive = false;
    let phoneWheelInteractionUntil = 0;
    let phoneInteractionResumeTimer = 0;
    let phoneActiveScrollRegionKey = "";
    let lastOverlayAppState = "";
    let tileContextMenuState = null;
    let tileContextMenuRenderSignature = "";
    let localActionProgressState = null;
    let placementFailureToastState = null;
    let viewerCompatibilityWarning = "";
    let placementCursorSendInFlight = false;
    let lastSentPlacementCursorSignature = "";
    let animationTimeSeconds = 0;
    let fpsSampleFrameCount = 0;
    let fpsSampleElapsedSeconds = 0;
    let fpsSampleWebWorkMilliseconds = 0;
    let framePerfSampleCount = 0;
    let framePerfSampleVisualHostMilliseconds = 0;
    let framePerfSampleGameplayDllMilliseconds = 0;
    let latestFramePerfSample = null;
    let latestFramePerfSampleTimestampMs = 0;
    let rendererWidth = 0;
    let rendererHeight = 0;
    let weatherPostProcess = null;
    let smoothedWeatherVisualResponse = {
        heatLevel: 0,
        dustLevel: 0,
        windLevel: 0,
        windDirectionDegrees: 0
    };
    let lastProcessedOneShotCueSequenceId = -1;
    let cameraOrbitDragPointerId = null;
    let cameraOrbitDragButton = -1;
    let cameraOrbitDragLastClientX = 0;
    let cameraOrbitPointerLockActive = false;
    let pendingSiteOrbitGesture = null;
    const orbitMouseButton = 2;
    const orbitMouseButtonsMask = 2;
    const orbitDragThresholdPixels = 6;
    const heatColorAddGainForVfx = 1.0;
    const witheringAlertDurationSeconds = 2.0;
    const witheringAlertDensityDropThreshold = 0.01;
    const witheringAlertHeightOffset = 0.04;
    const witheringAlertMaxOpacity = 0.72;
    const densityDecreaseAlertStyle = {
        colorHex: 0xff6b55,
        emissiveHex: 0xdb2414,
        emissiveBaseIntensity: 0.75,
        emissiveBoostIntensity: 3.1
    };
    const densityIncreaseAlertStyle = {
        colorHex: 0x61d97a,
        emissiveHex: 0x1e8e3e,
        emissiveBaseIntensity: 0.55,
        emissiveBoostIntensity: 2.6
    };
    const constantWitheringPlantTypeIds = new Set([
        5
    ]);
    const hudWarningCodes = {
        none: 0,
        windWatch: 1,
        windExposure: 2,
        severeGale: 3,
        sandblast: 4
    };
    const protectionOverlayModes = {
        none: 0,
        wind: 1,
        heat: 2,
        dust: 3
    };
    const highwayTerrainTypeId = 9001;
    const audioWarningHazardLevels = {
        0: 0.0,
        1: 0.35,
        2: 0.58,
        3: 0.82,
        4: 1.0
    };
    const siteTutorialTips = [
        "Move with WASD and drag with the right mouse button to orbit the camera around the worker.",
        "Press B to open the player pack, then click a carried item to use it or move it into opened storage.",
        "Press F to raise the phone for the market, delivery crate flow, and task board information.",
        "Short right-click any tile to open context actions such as harvest, water, repair, plant, or build.",
        "Planting and deployment enter placement mode first, so confirm with a left-click and cancel with Esc or a short right-click.",
        "Carry plantables such as seeds or straw checkerboard in the pack before you deploy so the right-click tile menu can arm planting actions.",
        "Open crates, benches, and other devices from the map to move items one click at a time through the pack.",
        "Wind, heat, and dust drain the worker over time, so use shelter and cover to limit exposure.",
        "Health, hydration, energy, and morale all matter; consumables and safer routing help stabilize them.",
        "Task progress and site completion rise as you restore tiles, place useful structures, and keep supplies flowing.",
        "The camp and nearby devices are your safe logistics hub for storage, crafting, and recovery planning.",
        "Delivery box items arrive outside the carried pack, so check it after shopping or ordered support arrives."
    ];

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
    const weatherDistortionVertexShader = `
        varying vec2 vUv;

        void main() {
            vUv = uv;
            gl_Position = vec4(position.xy, 0.0, 1.0);
        }
    `;
    const weatherDistortionFragmentShader = `
        precision highp float;

        uniform sampler2D tSceneColor;
        uniform vec2 uResolution;
        uniform float uTime;
        uniform float uHeatLevel;
        uniform float uHeatColorAddGain;
        uniform float uDustLevel;

        varying vec2 vUv;

        float hash12(vec2 p) {
            vec3 p3 = fract(vec3(p.xyx) * 0.1031);
            p3 += dot(p3, p3.yzx + 33.33);
            return fract((p3.x + p3.y) * p3.z);
        }

        float noise2d(vec2 p) {
            vec2 i = floor(p);
            vec2 f = fract(p);
            float a = hash12(i);
            float b = hash12(i + vec2(1.0, 0.0));
            float c = hash12(i + vec2(0.0, 1.0));
            float d = hash12(i + vec2(1.0, 1.0));
            vec2 u = f * f * (3.0 - 2.0 * f);
            return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
        }

        float fbm(vec2 p) {
            float value = 0.0;
            float amplitude = 0.5;
            for (int octave = 0; octave < 4; ++octave) {
                value += noise2d(p) * amplitude;
                p = p * 2.03 + vec2(17.1, 9.7);
                amplitude *= 0.5;
            }
            return value;
        }

        void main() {
            vec2 uv = vUv;
            vec2 texel = 1.0 / max(uResolution, vec2(1.0));
            vec4 baseSceneColor = texture2D(tSceneColor, uv);
            float heatAmount = clamp(uHeatLevel, 0.0, 1.0);
            float dustAmount = clamp(uDustLevel, 0.0, 1.0);

            float heatWaveA = sin(uv.y * 34.0 - uTime * 2.0 + uv.x * 8.0);
            float heatWaveB = sin(uv.y * 70.0 + uTime * 3.0 - uv.x * 5.0);
            vec2 distortion = vec2(
                heatWaveA * 0.75 + heatWaveB * 0.25,
                heatWaveB * 0.18);
            vec2 sampleUv = clamp(
                uv + distortion * texel * heatAmount * 6.0,
                vec2(0.001),
                vec2(0.999));
            vec3 distortedColor = texture2D(tSceneColor, sampleUv).rgb;
            vec3 heatSunOnSandColor = vec3(0.88, 0.66, 0.36);
            vec3 color = distortedColor;
            color += heatSunOnSandColor * (heatAmount * uHeatColorAddGain);

            vec3 dustTint = vec3(0.78, 0.67, 0.52);
            float horizonMask = smoothstep(0.10, 0.98, uv.y);
            float dustNoise = fbm(uv * vec2(3.2, 7.4) + vec2(-uTime * 0.03, uTime * 0.01));
            float opticalDepth =
                dustAmount *
                horizonMask *
                mix(0.45, 1.0, dustNoise);
            float transmittance = exp(-opticalDepth);
            vec3 mieFog = dustTint * (1.0 - transmittance);
            color = color * transmittance + mieFog;
            color = min(color, vec3(1.0));

            gl_FragColor = vec4(color, baseSceneColor.a);
            #include <colorspace_fragment>
        }
    `;
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
    const siteActionRequestFlags = {
        hasItem: 4,
        deferredTargetSelection: 8
    };
    const visibleWindStreakThreshold = 0.04;
    const siteActionCancelFlags = {
        currentAction: 1,
        placementMode: 2
    };
    const inventoryContainerCodes = {
        WORKER_PACK: 0,
        DEVICE_STORAGE: 1
    };
    const inventoryStorageFlags = {
        deliveryBox: 1,
        retrievalOnly: 2
    };
    const phoneDeliveryFee = 5.0;
    function formatMoney(value) {
        const normalized = Number.isFinite(value) ? value : 0;
        return normalized.toFixed(2);
    }
    let inventoryCache = createEmptyInventoryCache();
    const itemCatalog = {
        1: { name: "Water", shortName: "H2O", stackSize: 5, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        2: { name: "Food", shortName: "Ration", stackSize: 5, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        3: { name: "Medicine", shortName: "Med", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Use" },
        4: { name: "Ordos Wormwood Seeds", shortName: "Ordos Wormwood", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        5: { name: "White Thorn Seeds", shortName: "White Thorn", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        6: { name: "Red Tamarisk Seeds", shortName: "Red Tamarisk", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        7: { name: "Ningxia Wolfberry Seeds", shortName: "Ningxia Wolfberry", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        8: { name: "Wood", shortName: "Wood", stackSize: 20, canUse: false, canPlant: false, canDeploy: false },
        9: { name: "Iron", shortName: "Iron", stackSize: 20, canUse: false, canPlant: false, canDeploy: false },
        10: { name: "Dried Grass", shortName: "Grass", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        11: { name: "Camp Stove Kit", shortName: "Stove Kit", stackSize: 1, canUse: false, canPlant: false, canDeploy: true, deployStructureId: 201 },
        12: { name: "Workbench Kit", shortName: "Bench Kit", stackSize: 1, canUse: false, canPlant: false, canDeploy: true, deployStructureId: 202 },
        13: { name: "Storage Crate Kit", shortName: "Crate Kit", stackSize: 1, canUse: false, canPlant: false, canDeploy: true, deployStructureId: 203 },
        14: { name: "Hammer", shortName: "Hammer", stackSize: 1, canUse: false, canPlant: false, canDeploy: false },
        15: { name: "Basic Straw Checkerboard", shortName: "Checkerboard", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        16: { name: "Korshinsk Peashrub Seeds", shortName: "Peashrub", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        17: { name: "Jiji Grass Seeds", shortName: "Jiji Grass", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        18: { name: "Sea Buckthorn Seeds", shortName: "Sea Buckthorn", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        19: { name: "Desert Ephedra Seeds", shortName: "Ephedra", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        20: { name: "Saxaul Seeds", shortName: "Saxaul", stackSize: 10, canUse: false, canPlant: true, canDeploy: false },
        21: { name: "White Thorn Berries", shortName: "White Thorn", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        22: { name: "Red Tamarisk Bark", shortName: "Bark", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        23: { name: "Ningxia Wolfberries", shortName: "Wolfberries", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        24: { name: "Korshinsk Peashrub Pods", shortName: "Peapods", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        25: { name: "Jiji Grass Fiber", shortName: "Fiber", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        26: { name: "Sea Buckthorn Berries", shortName: "Buckthorn", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        27: { name: "Desert Ephedra Sprigs", shortName: "Ephedra", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        28: { name: "Saxaul Fuelwood", shortName: "Fuelwood", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        29: { name: "Wind-Polished Desert Pebble", shortName: "Pebble", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        30: { name: "Desert Jasper", shortName: "Jasper", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        31: { name: "Black Gobi Stone", shortName: "Gobi Stone", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        32: { name: "Gobi Agate", shortName: "Agate", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        33: { name: "Alxa Agate", shortName: "Alxa", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        34: { name: "Turquoise Vein Fragment", shortName: "Turquoise", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        35: { name: "Golden Silk Jade", shortName: "Silk Jade", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        36: { name: "Hetian Jade Pebble", shortName: "Hetian Jade", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        40: { name: "Ordos Wormwood Sprigs", shortName: "Sprigs", stackSize: 10, canUse: false, canPlant: false, canDeploy: false },
        41: { name: "Shovel", shortName: "Shovel", stackSize: 1, canUse: false, canPlant: false, canDeploy: false },
        42: { name: "Wormwood Broth", shortName: "Broth", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        43: { name: "Thornberry Cooler", shortName: "Cooler", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        44: { name: "Peashrub Hotpot", shortName: "Hotpot", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        45: { name: "Buckthorn Tonic", shortName: "Tonic", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        46: { name: "Jadeleaf Stew", shortName: "Jadeleaf", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        47: { name: "Desert Revival Draught", shortName: "Revival", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        48: { name: "Rich Wormwood Broth", shortName: "Rich Broth", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        49: { name: "Rich Thornberry Cooler", shortName: "Rich Cooler", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        50: { name: "Rich Peashrub Hotpot", shortName: "Rich Hotpot", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        51: { name: "Rich Buckthorn Tonic", shortName: "Rich Tonic", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        52: { name: "Rich Jadeleaf Stew", shortName: "Rich Stew", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" },
        53: { name: "Rich Desert Revival Draught", shortName: "Rich Revival", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Drink" },
        54: { name: "Ephedra Stew", shortName: "Ephedra Stew", stackSize: 3, canUse: true, canPlant: false, canDeploy: false, useLabel: "Eat" }
    };
    const itemVisuals = {
        1: { iconKey: "water", light: "#5d8eb3", dark: "#33546f" },
        2: { iconKey: "ration", light: "#b48a4f", dark: "#6f4a28" },
        3: { iconKey: "medicine", light: "#9a676c", dark: "#62363b" },
        4: { iconKey: "reed", light: "#79a35f", dark: "#46693a" },
        5: { iconKey: "shrub", light: "#8aa05c", dark: "#53683a" },
        6: { iconKey: "cactus", light: "#6a8c67", dark: "#3c5841" },
        7: { iconKey: "vine", light: "#8f7c4c", dark: "#5c4526" },
        8: { iconKey: "wood", light: "#ac7d4b", dark: "#6e4a28" },
        9: { iconKey: "iron", light: "#9ca7b4", dark: "#596572" },
        10: { iconKey: "fiber", light: "#a2bb7b", dark: "#607541" },
        11: { iconKey: "flame", light: "#c7835a", dark: "#7b4327" },
        12: { iconKey: "hammer", light: "#b2966c", dark: "#6b5437" },
        13: { iconKey: "box", light: "#af8c63", dark: "#694d2c" },
        14: { iconKey: "hammer", light: "#9ea9b7", dark: "#5c6775" },
        15: { iconKey: "grid", light: "#c8a46a", dark: "#7d5c31" },
        16: { iconKey: "shrub", light: "#87a46a", dark: "#4c6843" },
        17: { iconKey: "reed", light: "#b0b86d", dark: "#676f3a" },
        18: { iconKey: "vine", light: "#c08b42", dark: "#7d5526" },
        19: { iconKey: "cactus", light: "#8ea277", dark: "#526442" },
        20: { iconKey: "wood", light: "#9c8259", dark: "#604a2d" },
        21: { iconKey: "vine", light: "#c9a255", dark: "#7c5628" },
        22: { iconKey: "wood", light: "#b57f58", dark: "#73472a" },
        23: { iconKey: "vine", light: "#c45d4b", dark: "#7b3127" },
        24: { iconKey: "shrub", light: "#93a85d", dark: "#556a33" },
        25: { iconKey: "fiber", light: "#c7c46d", dark: "#74703b" },
        26: { iconKey: "vine", light: "#d08935", dark: "#844c22" },
        27: { iconKey: "reed", light: "#91ad69", dark: "#526540" },
        28: { iconKey: "wood", light: "#9f8051", dark: "#624b2d" },
        29: { iconKey: "grid", light: "#b19a7a", dark: "#66553e" },
        30: { iconKey: "grid", light: "#ba8c63", dark: "#72472c" },
        31: { iconKey: "grid", light: "#89858d", dark: "#4d4a52" },
        32: { iconKey: "grid", light: "#d0a162", dark: "#7f562a" },
        33: { iconKey: "grid", light: "#d68557", dark: "#7d3f25" },
        34: { iconKey: "grid", light: "#67b8b5", dark: "#2d6667" },
        35: { iconKey: "grid", light: "#d3c06d", dark: "#7b6528" },
        36: { iconKey: "grid", light: "#dfe4e8", dark: "#7c8388" },
        40: { iconKey: "reed", light: "#8db56b", dark: "#507041" },
        41: { iconKey: "hammer", light: "#a8b0bc", dark: "#616977" },
        42: { iconKey: "ration", light: "#b2a06f", dark: "#67552f" },
        43: { iconKey: "water", light: "#75a7c2", dark: "#3c6277" },
        44: { iconKey: "ration", light: "#be8958", dark: "#704829" },
        45: { iconKey: "water", light: "#d39a52", dark: "#7c4f25" },
        46: { iconKey: "ration", light: "#9db46b", dark: "#5d6e39" },
        47: { iconKey: "water", light: "#98c7b0", dark: "#4d7562" },
        48: { iconKey: "ration", light: "#c3b07d", dark: "#72613a" },
        49: { iconKey: "water", light: "#89b8d0", dark: "#466b80" },
        50: { iconKey: "ration", light: "#cf965e", dark: "#7a4b2a" },
        51: { iconKey: "water", light: "#e0aa63", dark: "#865629" },
        52: { iconKey: "ration", light: "#afc578", dark: "#657541" },
        53: { iconKey: "water", light: "#acd7c0", dark: "#587f6e" },
        54: { iconKey: "ration", light: "#9eb67c", dark: "#5f7443" }
    };
    const uiIconMarkup = {
        water: [
            '<path d="M12 3C9.6 6.6 7 9.8 7 13a5 5 0 0 0 10 0c0-3.2-2.6-6.4-5-10Z"></path>',
            '<path d="M10 14.5c.5.9 1.2 1.4 2.3 1.7"></path>'
        ].join(""),
        ration: [
            '<path d="M6 13h12"></path>',
            '<path d="M7 13a5 5 0 0 0 10 0"></path>',
            '<path d="M10 9c0-1 .6-1.8 1.4-2.8"></path>',
            '<path d="M13 9c0-.8.5-1.6 1.2-2.4"></path>'
        ].join(""),
        medicine: [
            '<rect x="5" y="5" width="14" height="14" rx="4"></rect>',
            '<path d="M12 8v8"></path>',
            '<path d="M8 12h8"></path>'
        ].join(""),
        reed: [
            '<path d="M8 19c0-4 .5-7.2 2-10"></path>',
            '<path d="M12 19c0-5 1.8-8.3 4.4-11"></path>',
            '<path d="M10 10c-1.4 0-2.7-.6-3.8-1.8"></path>',
            '<path d="M15.8 8.5c.2-1.6.9-2.9 2.2-4"></path>'
        ].join(""),
        shrub: [
            '<path d="M12 19v-4"></path>',
            '<path d="M12 15c-3.2 0-5.8-2.4-6.2-5.8 3.4 0 5.7 1.7 6.2 5.8Z"></path>',
            '<path d="M12 15c.4-4.1 2.8-5.8 6.2-5.8-.4 3.4-3 5.8-6.2 5.8Z"></path>',
            '<path d="M12 12.5c-2.3 0-4.1-1.6-4.5-4 2.3 0 4.1 1.2 4.5 4Z"></path>'
        ].join(""),
        cactus: [
            '<path d="M12 5v14"></path>',
            '<path d="M12 10h-2.5a2.5 2.5 0 0 0-2.5 2.5V15"></path>',
            '<path d="M12 12h2.5A2.5 2.5 0 0 1 17 14.5V16"></path>',
            '<path d="M9 19h6"></path>'
        ].join(""),
        vine: [
            '<path d="M12 19v-4.5c0-2.6 1.7-4.8 4.2-5.4"></path>',
            '<path d="M12 13c-2.9 0-5-1.8-5.6-4.8 2.8 0 4.9 1.5 5.6 4.8Z"></path>',
            '<circle cx="16.8" cy="8.2" r="1.8"></circle>',
            '<path d="M16.8 10v3.2"></path>'
        ].join(""),
        wood: [
            '<rect x="4.5" y="7" width="10" height="4" rx="2"></rect>',
            '<rect x="9.5" y="13" width="10" height="4" rx="2"></rect>',
            '<circle cx="13" cy="9" r="1.2"></circle>',
            '<circle cx="11" cy="15" r="1.2"></circle>'
        ].join(""),
        iron: [
            '<path d="M7 9h10l2 4-2 4H7l-2-4 2-4Z"></path>',
            '<path d="M9 9l-2 4 2 4"></path>',
            '<path d="M15 9l2 4-2 4"></path>'
        ].join(""),
        fiber: [
            '<path d="M12 19V8"></path>',
            '<path d="M9 18V10"></path>',
            '<path d="M15 18V10"></path>',
            '<path d="M9 11 7 9"></path>',
            '<path d="M9 13 7 11"></path>',
            '<path d="M15 11 17 9"></path>',
            '<path d="M15 13 17 11"></path>',
            '<path d="M9 19h6"></path>'
        ].join(""),
        flame: [
            '<path d="M12 4c1.7 2 3 3.7 3 6a3 3 0 1 1-6 0c0-2 1-3.7 3-6Z"></path>',
            '<path d="M12 10.2c.9 1 1.3 1.8 1.3 2.7a1.3 1.3 0 1 1-2.6 0c0-.9.4-1.7 1.3-2.7Z"></path>'
        ].join(""),
        hammer: [
            '<path d="M14.5 5.5 18.5 9.5"></path>',
            '<path d="M13.2 6.8 7 13"></path>',
            '<path d="M7 13 5 18l5-2"></path>',
            '<path d="M15.5 4.5 17.5 2.5"></path>'
        ].join(""),
        box: [
            '<path d="m4 9 8-4 8 4-8 4-8-4Z"></path>',
            '<path d="M4 9v6l8 4 8-4V9"></path>',
            '<path d="M12 13v6"></path>'
        ].join(""),
        grid: [
            '<rect x="5" y="5" width="14" height="14" rx="3"></rect>',
            '<path d="M12 5v14"></path>',
            '<path d="M5 12h14"></path>'
        ].join(""),
        sprout: [
            '<path d="M12 19v-5"></path>',
            '<path d="M12 14c-3 0-5-2-5.5-5 3 0 5 2 5.5 5Z"></path>',
            '<path d="M12 14c.5-3 2.5-5 5.5-5-.5 3-2.5 5-5.5 5Z"></path>'
        ].join(""),
        craft: [
            '<path d="M14.2 7.2 17.8 10.8"></path>',
            '<path d="M13 8.4 7 14.4"></path>',
            '<path d="M7 14.4 5.2 18.8 9.6 17"></path>',
            '<path d="M17.5 4.5v3"></path>',
            '<path d="M16 6h3"></path>'
        ].join(""),
        camp: [
            '<path d="M6 10.5 12 5l6 5.5"></path>',
            '<path d="M8 10.5V18h8v-7.5"></path>',
            '<path d="M11 18v-4h2v4"></path>'
        ].join(""),
        idle: [
            '<circle cx="12" cy="12" r="7"></circle>',
            '<path d="M8 16 16 8"></path>'
        ].join(""),
        checklist: [
            '<path d="M9.5 7H18"></path>',
            '<path d="M9.5 12H18"></path>',
            '<path d="M9.5 17H18"></path>',
            '<path d="m5 7 1.4 1.4L8.4 6.4"></path>',
            '<path d="m5 12 1.4 1.4 2-2"></path>',
            '<path d="m5 17 1.4 1.4 2-2"></path>'
        ].join(""),
        cart: [
            '<path d="M4.5 6H7l1.6 7h8.2l1.7-5.5H8.2"></path>',
            '<circle cx="10" cy="17.5" r="1.3"></circle>',
            '<circle cx="16" cy="17.5" r="1.3"></circle>'
        ].join(""),
        coins: [
            '<path d="M7 8c0-1.1 2.2-2 5-2s5 .9 5 2-2.2 2-5 2-5-.9-5-2Z"></path>',
            '<path d="M7 8v4c0 1.1 2.2 2 5 2s5-.9 5-2V8"></path>',
            '<path d="M8.5 15.5c.8.4 2 .7 3.5.7 2.8 0 5-.9 5-2v-1"></path>'
        ].join(""),
        userPlus: [
            '<circle cx="10" cy="9" r="3"></circle>',
            '<path d="M4.5 18a5.5 5.5 0 0 1 11 0"></path>',
            '<path d="M18 8v6"></path>',
            '<path d="M15 11h6"></path>'
        ].join(""),
        wind: [
            '<path d="M4 9h10a2 2 0 1 0-2-2"></path>',
            '<path d="M4 13h14a2 2 0 1 1-2 2"></path>',
            '<path d="M4 17h8"></path>'
        ].join(""),
        compass: [
            '<circle cx="12" cy="12" r="7"></circle>',
            '<path d="m10 14 1.7-4.7L16.5 7.5 14.7 12.3 10 14Z"></path>'
        ].join(""),
        lock: [
            '<rect x="6.5" y="10.5" width="11" height="9.5" rx="2.5"></rect>',
            '<path d="M9 10.5V8.9a3 3 0 0 1 6 0v1.6"></path>',
            '<path d="M12 14.3v2.4"></path>'
        ].join("")
    };
    const phoneAppVisuals = {
        Tasks: { iconKey: "checklist", light: "#7e8e61", dark: "#4f6140" },
        Buy: { iconKey: "cart", light: "#6d8ea1", dark: "#3f5c6a" },
        Sell: { iconKey: "coins", light: "#b98f58", dark: "#7a592d" },
        Services: { iconKey: "userPlus", light: "#a37c93", dark: "#694d60" }
    };
    const structureCatalog = {
        201: { name: "Camp Stove", shortName: "Stove", slotCount: 6 },
        202: { name: "Workbench", shortName: "Workbench", slotCount: 8 },
        203: { name: "Storage Crate", shortName: "Crate", slotCount: 10 }
    };
    const plantFootprintCatalog = Object.freeze({
        1: { width: 2, height: 2 },
        2: { width: 2, height: 2 },
        3: { width: 2, height: 2 },
        4: { width: 2, height: 2 },
        5: { width: 2, height: 2 },
        6: { width: 2, height: 2 },
        7: { width: 2, height: 2 },
        8: { width: 2, height: 2 },
        9: { width: 2, height: 2 },
        10: { width: 2, height: 2 }
    });
    const harvestablePlantCatalog = Object.freeze({
        1: { itemId: 10, lootDensityThreshold: 0.25 },
        2: { itemId: 21, lootDensityThreshold: 0.25 },
        3: { itemId: 22, lootDensityThreshold: 0.25 },
        4: { itemId: 23, lootDensityThreshold: 0.25 },
        6: { itemId: 10, lootDensityThreshold: 0.25 },
        7: { itemId: 10, lootDensityThreshold: 0.25 },
        8: { itemId: 26, lootDensityThreshold: 0.25 },
        9: { itemId: 10, lootDensityThreshold: 0.25 },
        10: { itemId: 10, lootDensityThreshold: 0.25 }
    });
    const plantWindShaderUniforms = {
        uPlantTime: { value: 0.0 },
        uPlantWindStrength: { value: 0.0 },
        uPlantWindDirection: { value: new THREE_NS.Vector2(0.0, 1.0) }
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
    const audioManager = createAudioManagerState();

    const renderer = new THREE_NS.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.outputColorSpace = THREE_NS.SRGBColorSpace;
    renderer.domElement.style.touchAction = "none";
    gameView.appendChild(renderer.domElement);
    const dustOverlay = document.createElement("div");
    dustOverlay.style.position = "absolute";
    dustOverlay.style.inset = "-8%";
    dustOverlay.style.pointerEvents = "none";
    dustOverlay.style.zIndex = "4";
    dustOverlay.style.opacity = "0";
    dustOverlay.style.transition = "opacity 140ms linear";
    dustOverlay.style.backgroundImage =
        "radial-gradient(ellipse at 22% 62%, rgba(208, 177, 129, 0.32) 0%, rgba(208, 177, 129, 0.18) 24%, rgba(208, 177, 129, 0.00) 58%)," +
        "radial-gradient(ellipse at 74% 48%, rgba(190, 156, 108, 0.28) 0%, rgba(190, 156, 108, 0.12) 28%, rgba(190, 156, 108, 0.00) 62%)," +
        "linear-gradient(180deg, rgba(232, 209, 174, 0.04) 0%, rgba(220, 189, 145, 0.12) 38%, rgba(198, 160, 113, 0.22) 72%, rgba(174, 136, 92, 0.34) 100%)";
    dustOverlay.style.backgroundSize = "150% 140%, 145% 135%, 100% 100%";
    dustOverlay.style.filter = "blur(28px) saturate(0.92)";
    dustOverlay.style.mixBlendMode = "normal";
    gameView.appendChild(dustOverlay);

    const scene = new THREE_NS.Scene();
    scene.background = new THREE_NS.Color(0xf4e6cf);

    const camera = new THREE_NS.PerspectiveCamera(45, 1, 0.1, 2000);
    const ambient = new THREE_NS.AmbientLight(0xffffff, 1.3);
    const directional = new THREE_NS.DirectionalLight(0xffffff, 1.0);
    directional.position.set(12, 18, 10);
    scene.add(ambient, directional);

    const worldGroup = new THREE_NS.Group();
    scene.add(worldGroup);

    weatherPostProcess = createWeatherDistortionPostProcess();

    function fitRenderer() {
        const width = Math.max(gameView.clientWidth, 320);
        const height = Math.max(gameView.clientHeight, 240);
        if (width === rendererWidth && height === rendererHeight) {
            return;
        }

        rendererWidth = width;
        rendererHeight = height;
        renderer.setSize(width, height, false);
        resizeWeatherDistortionPostProcess(weatherPostProcess, width, height);
        camera.aspect = width / height;
        camera.updateProjectionMatrix();
    }

    function createWeatherDistortionPostProcess() {
        const renderTarget = new THREE_NS.WebGLRenderTarget(1, 1, {
            minFilter: THREE_NS.LinearFilter,
            magFilter: THREE_NS.LinearFilter,
            generateMipmaps: false,
            stencilBuffer: false
        });
        if (renderer.capabilities.isWebGL2) {
            renderTarget.samples = 2;
        }

        const material = new THREE_NS.ShaderMaterial({
            uniforms: {
                tSceneColor: { value: renderTarget.texture },
                uResolution: { value: new THREE_NS.Vector2(1, 1) },
                uTime: { value: 0 },
                uHeatLevel: { value: 0 },
                uHeatColorAddGain: { value: heatColorAddGainForVfx },
                uDustLevel: { value: 0 },
            },
            vertexShader: weatherDistortionVertexShader,
            fragmentShader: weatherDistortionFragmentShader,
            depthWrite: false,
            depthTest: false
        });

        const postScene = new THREE_NS.Scene();
        const postCamera = new THREE_NS.OrthographicCamera(-1, 1, 1, -1, 0, 1);
        const postQuad = new THREE_NS.Mesh(new THREE_NS.PlaneGeometry(2, 2), material);
        postQuad.frustumCulled = false;
        postScene.add(postQuad);

        return {
            renderTarget,
            material,
            scene: postScene,
            camera: postCamera
        };
    }

    function resizeWeatherDistortionPostProcess(postProcess, width, height) {
        if (!postProcess) {
            return;
        }

        postProcess.renderTarget.setSize(Math.max(width, 1), Math.max(height, 1));
        postProcess.material.uniforms.uResolution.value.set(Math.max(width, 1), Math.max(height, 1));
    }

    function clamp01(value) {
        return Math.max(0, Math.min(value, 1));
    }

    function smoothstep01(edge0, edge1, value) {
        if (edge0 === edge1) {
            return value < edge0 ? 0 : 1;
        }
        const t = clamp01((value - edge0) / (edge1 - edge0));
        return t * t * (3.0 - 2.0 * t);
    }

    function normalizeWeatherValue(value) {
        return clamp01((typeof value === "number" ? value : 0) / 100.0);
    }

    function normalizeWindVisualValue(value) {
        return clamp01((typeof value === "number" ? value : 0) / 10.0);
    }

    function blendToward(current, target, ratePerSecond, deltaSeconds) {
        if (!(deltaSeconds > 0)) {
            return target;
        }
        const blend = 1.0 - Math.exp(-ratePerSecond * deltaSeconds);
        return current + (target - current) * blend;
    }

    function normalizeAngleDegrees(angle) {
        let normalized = typeof angle === "number" ? angle : 0;
        while (normalized >= 360.0) {
            normalized -= 360.0;
        }
        while (normalized < 0.0) {
            normalized += 360.0;
        }
        return normalized;
    }

    function blendAngleDegreesToward(current, target, ratePerSecond, deltaSeconds) {
        if (!(deltaSeconds > 0)) {
            return normalizeAngleDegrees(target);
        }

        const normalizedCurrent = normalizeAngleDegrees(current);
        const normalizedTarget = normalizeAngleDegrees(target);
        let delta = normalizedTarget - normalizedCurrent;
        if (delta > 180.0) {
            delta -= 360.0;
        } else if (delta < -180.0) {
            delta += 360.0;
        }

        const blend = 1.0 - Math.exp(-ratePerSecond * deltaSeconds);
        return normalizeAngleDegrees(normalizedCurrent + delta * blend);
    }

    function buildTargetWeatherVisualResponse(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            return {
                heatLevel: 0,
                dustLevel: 0,
                windLevel: 0,
                windDirectionDegrees: smoothedWeatherVisualResponse.windDirectionDegrees
            };
        }

        const siteState = getSiteState(state);
        const weather = siteState ? siteState.weather : null;
        if (!weather) {
            return {
                heatLevel: 0,
                dustLevel: 0,
                windLevel: 0,
                windDirectionDegrees: smoothedWeatherVisualResponse.windDirectionDegrees
            };
        }

        return {
            heatLevel: normalizeWeatherValue(weather.heat),
            dustLevel: normalizeWeatherValue(weather.dust),
            windLevel: normalizeWindVisualValue(weather.wind),
            windDirectionDegrees:
                typeof weather.windDirectionDegrees === "number"
                    ? normalizeAngleDegrees(weather.windDirectionDegrees)
                    : smoothedWeatherVisualResponse.windDirectionDegrees
        };
    }

    function updateSmoothedWeatherVisualResponse(state, deltaSeconds) {
        const target = buildTargetWeatherVisualResponse(state);
        const blendRate = 6.0;

        function blendWeatherVisualChannel(current, nextTarget) {
            return clamp01(blendToward(current, nextTarget, blendRate, deltaSeconds));
        }

        smoothedWeatherVisualResponse.heatLevel =
            blendWeatherVisualChannel(smoothedWeatherVisualResponse.heatLevel, target.heatLevel);
        smoothedWeatherVisualResponse.dustLevel =
            blendWeatherVisualChannel(smoothedWeatherVisualResponse.dustLevel, target.dustLevel);
        smoothedWeatherVisualResponse.windLevel =
            blendWeatherVisualChannel(smoothedWeatherVisualResponse.windLevel, target.windLevel);
        smoothedWeatherVisualResponse.windDirectionDegrees = blendAngleDegreesToward(
            smoothedWeatherVisualResponse.windDirectionDegrees,
            target.windDirectionDegrees,
            blendRate,
            deltaSeconds);
    }

    function getWeatherVisualResponse() {
        return smoothedWeatherVisualResponse;
    }

    function resolveVisualWindDirectionDegrees() {
        return normalizeAngleDegrees(getWeatherVisualResponse().windDirectionDegrees || 0);
    }

    function getHudWarningPresentation(state) {
        const hud = getHudState(state);
        const siteState = getSiteState(state);
        const weather = siteState ? siteState.weather : null;
        const warningCode = hud && typeof hud.warningCode === "number"
            ? hud.warningCode
            : hudWarningCodes.none;
        const wind = weather && typeof weather.wind === "number" ? Math.round(weather.wind) : 0;
        const dust = weather && typeof weather.dust === "number" ? Math.round(weather.dust) : 0;
        const windDirection =
            weather && typeof weather.windDirectionDegrees === "number"
                ? Math.round(weather.windDirectionDegrees)
                : 0;
        const eventTemplateId =
            weather && typeof weather.eventTemplateId === "number"
                ? weather.eventTemplateId
                : 0;
        const weatherBrief = "Wind " + wind + " @" + windDirection + "deg  |  Dust " + dust +
            (eventTemplateId !== 0 ? ("  |  Event " + eventTemplateId) : "");

        switch (warningCode) {
        case hudWarningCodes.windWatch:
            return {
                code: warningCode,
                headline: "Wind Building",
                detail: "Crosswind is pushing grit across the site. " + weatherBrief + "."
            };
        case hudWarningCodes.windExposure:
            return {
                code: warningCode,
                headline: "Wind Exposure",
                detail: "Open-ground work is slowing and worker drain is rising. " + weatherBrief + "."
            };
        case hudWarningCodes.severeGale:
            return {
                code: warningCode,
                headline: "Severe Gale",
                detail: "Strong gusts are cutting action throughput hard. Find cover. " + weatherBrief + "."
            };
        case hudWarningCodes.sandblast:
            return {
                code: warningCode,
                headline: "Sandblast Front",
                detail: "Wind and airborne grit are peaking. Shelter now. " + weatherBrief + "."
            };
        default:
            return {
                code: hudWarningCodes.none,
                headline: "Field Stable",
                detail: weather ? ("Local weather is manageable. " + weatherBrief + ".") : "Weather telemetry is quiet."
            };
        }
    }

    function getWindVisualResponse(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            return {
                opacity: 0,
                dustAlpha: 0,
                speedSeconds: 4.8,
                tiltDegrees: -18,
                directionDegrees: 0,
                severity: "none"
            };
        }

        const siteState = getSiteState(state);
        if (!siteState || !siteState.weather) {
            return {
                opacity: 0,
                dustAlpha: 0,
                speedSeconds: 4.8,
                tiltDegrees: -18,
                directionDegrees: 0,
                severity: "none"
            };
        }

        const weatherVisualResponse = getWeatherVisualResponse();
        const windLevel = weatherVisualResponse.windLevel;
        const windDensityLevel = Math.pow(clamp01(windLevel), 1.35);
        const windSpeedLevel = Math.pow(clamp01(windLevel), 1.18);
        const directionDegrees = resolveVisualWindDirectionDegrees();
        const opacity = Math.min(0.2, windDensityLevel * 0.2);
        const dustAlpha = Math.min(0.09, windDensityLevel * 0.09);
        let severity = "watch";
        if (windLevel >= 0.8) {
            severity = "critical";
        } else if (windLevel >= 0.56) {
            severity = "severe";
        } else if (windLevel >= 0.35) {
            severity = "exposure";
        } else if (opacity <= 0.04) {
            severity = "none";
        }

        return {
            opacity: opacity,
            dustAlpha: dustAlpha,
            speedSeconds: Math.max(4.17, 5.4 - windSpeedLevel * 1.23),
            tiltDegrees: directionDegrees - 90.0,
            directionDegrees: directionDegrees,
            severity: severity
        };
    }

    function formatViewerError(error) {
        if (!error) {
            return "Unknown error";
        }
        if (typeof error === "string") {
            return error;
        }
        if (error && typeof error.message === "string" && error.message.length > 0) {
            return error.message;
        }
        return String(error);
    }

    function reportViewerError(tag, error) {
        const message = formatViewerError(error);
        postViewerLog("error", tag, {
            message: message,
            stack: error && error.stack ? String(error.stack) : ""
        });
        if (typeof console !== "undefined" && typeof console.error === "function") {
            console.error("[viewer-error]", tag, error);
        }
        if (statusChip) {
            statusChip.textContent = "Viewer Error\n" + tag + "\n" + message;
        }
    }

    function postViewerLog(level, tag, details) {
        const payload = {
            level: level,
            tag: tag,
            frameNumber: latestState && typeof latestState.frameNumber === "number"
                ? latestState.frameNumber
                : null,
            appState: latestState && latestState.appState ? latestState.appState : null,
            details: details || {}
        };

        try {
            postJson("/client-log", payload).catch(function () {});
        } catch (_error) {
        }
    }

    function applyWindOverlay(state) {
        if (!windOverlay) {
            return;
        }

        const response = getWindVisualResponse(state);
        const isActive = response.opacity > 0.02;
        const directionRadians = (response.directionDegrees || 0) * Math.PI / 180.0;
        const driftX = Math.sin(directionRadians) * 10.0;
        const driftY = -Math.cos(directionRadians) * 6.0;
        stageFrame.classList.toggle("wind-overlay-active", isActive);
        stageFrame.classList.toggle("wind-overlay-severe", response.severity === "severe" || response.severity === "critical");
        stageFrame.classList.toggle("wind-overlay-critical", response.severity === "critical");
        stageFrame.style.setProperty("--wind-overlay-opacity", response.opacity.toFixed(3));
        stageFrame.style.setProperty("--wind-overlay-dust-alpha", response.dustAlpha.toFixed(3));
        stageFrame.style.setProperty("--wind-overlay-speed", response.speedSeconds.toFixed(2) + "s");
        stageFrame.style.setProperty("--wind-overlay-tilt", response.tiltDegrees.toFixed(1) + "deg");
        stageFrame.style.setProperty("--wind-overlay-from-x", (-driftX).toFixed(2) + "%");
        stageFrame.style.setProperty("--wind-overlay-from-y", (-driftY).toFixed(2) + "%");
        stageFrame.style.setProperty("--wind-overlay-to-x", driftX.toFixed(2) + "%");
        stageFrame.style.setProperty("--wind-overlay-to-y", driftY.toFixed(2) + "%");
    }

    function renderSceneWithWeatherDistortion(state, elapsedSeconds) {
        const weatherVisualResponse = getWeatherVisualResponse();
        if (!weatherPostProcess ||
            (weatherVisualResponse.heatLevel <= 0.001 &&
                weatherVisualResponse.dustLevel <= 0.001)) {
            renderer.setRenderTarget(null);
            renderer.render(scene, camera);
            return;
        }

        weatherPostProcess.material.uniforms.uTime.value = elapsedSeconds;
        weatherPostProcess.material.uniforms.uHeatLevel.value = weatherVisualResponse.heatLevel;
        weatherPostProcess.material.uniforms.uHeatColorAddGain.value = heatColorAddGainForVfx;
        weatherPostProcess.material.uniforms.uDustLevel.value = weatherVisualResponse.dustLevel;
        weatherPostProcess.material.uniforms.tSceneColor.value = weatherPostProcess.renderTarget.texture;

        renderer.setRenderTarget(weatherPostProcess.renderTarget);
        renderer.render(scene, camera);
        renderer.setRenderTarget(null);
        renderer.render(weatherPostProcess.scene, weatherPostProcess.camera);
    }

    function updateDustOverlay(state, elapsedSeconds) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            dustOverlay.style.opacity = "0";
            return;
        }
        dustOverlay.style.opacity = "0";
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

    function refreshStateSnapshot() {
        return fetch("/state")
            .then((response) => response.json())
            .then((state) => {
                handleIncomingState(state, true);
            })
            .catch(() => {});
    }

    function makeButton(label, onClick, secondary, disabled, skipClickHandler) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "action-button" + (secondary ? " secondary" : "");
        button.textContent = label;
        button.disabled = !!disabled;
        if (!skipClickHandler) {
            button.addEventListener("click", onClick);
        }
        return button;
    }

    function bindReliablePrimaryPress(button, onPress) {
        let suppressClick = false;

        button.addEventListener("mousedown", function (event) {
            if (event.button !== 0 || button.disabled) {
                return;
            }

            suppressClick = true;
            phonePointerInteractionActive = true;
            phoneActiveScrollRegionKey = "";
            phoneWheelInteractionUntil = 0;
            event.preventDefault();
            event.stopPropagation();
            onPress(event);
        });

        button.addEventListener("click", function (event) {
            event.preventDefault();
            event.stopPropagation();

            if (suppressClick) {
                suppressClick = false;
                return;
            }

            if (!button.disabled) {
                onPress(event);
            }
        });
    }

    function makePhoneActionButton(label, onClick, secondary, disabled) {
        const button = makeButton(label, onClick, secondary, disabled, true);
        button.classList.add("phone-action-button");
        bindReliablePrimaryPress(button, onClick);
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

    function setTechTreeOverlayActive(active) {
        stageFrame.classList.toggle("tech-tree-overlay-active", !!active);
        selectionChip.classList.toggle("tech-tree-overlay", !!active);
        if (!active) {
            hideTechTooltip();
        }
    }

    function makeUiAction(type, targetId, arg0, arg1) {
        return {
            type: type,
            targetId: targetId || 0,
            arg0: arg0 || 0,
            arg1: arg1 || 0
        };
    }

    function postOpenSiteProtectionSelector() {
        return postJson("/ui-action", makeUiAction("OPEN_SITE_PROTECTION_SELECTOR", 0, 0, 0));
    }

    function postCloseSiteProtectionUi() {
        return postJson("/ui-action", makeUiAction("CLOSE_SITE_PROTECTION_UI", 0, 0, 0));
    }

    function postSetSiteProtectionOverlayMode(mode) {
        return postJson(
            "/ui-action",
            makeUiAction("SET_SITE_PROTECTION_OVERLAY_MODE", 0, mode, 0));
    }

    function postEndSiteModifier(modifierId) {
        return postJson(
            "/ui-action",
            makeUiAction("END_SITE_MODIFIER", modifierId, 0, 0));
    }

    function splitInfoParts(text) {
        return String(text || "")
            .split("|")
            .map((part) => part.trim())
            .filter(Boolean);
    }

    function getTechTreeCloseAction(techTreeSetup) {
        const backgroundElement = getBackgroundClickElement(techTreeSetup);
        if (backgroundElement && backgroundElement.action) {
            return backgroundElement.action;
        }
        return makeUiAction("CLOSE_REGIONAL_MAP_TECH_TREE", 0, 0, 0);
    }

    function getTechTreeSetup(state) {
        return getSetup(state, "REGIONAL_MAP_TECH_TREE");
    }

    function getSiteProtectionSelectorSetup(state) {
        return getSetup(state, "SITE_PROTECTION_SELECTOR");
    }

    function getSiteBootstrap(state) {
        return state.siteBootstrap || null;
    }

    function getSiteState(state) {
        return state.siteState || null;
    }

    function getActiveModifiers(state) {
        const siteState = getSiteState(state);
        return siteState && Array.isArray(siteState.activeModifiers)
            ? siteState.activeModifiers
            : [];
    }

    function getSiteAction(state) {
        return state && state.siteAction ? state.siteAction : null;
    }

    function isSiteActionActive(siteAction) {
        return !!siteAction &&
            typeof siteAction.actionKind === "number" &&
            siteAction.actionKind !== 0 &&
            typeof siteAction.flags === "number" &&
            (siteAction.flags & 1) !== 0;
    }

    function getActiveExcavationTile(siteAction) {
        if (!isSiteActionActive(siteAction) || siteAction.actionKind !== 10) {
            return null;
        }

        return {
            x: typeof siteAction.targetTileX === "number" ? siteAction.targetTileX : 0,
            y: typeof siteAction.targetTileY === "number" ? siteAction.targetTileY : 0
        };
    }

    function getSiteResult(state) {
        return state.siteResult || null;
    }

    function getProtectionOverlayState(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.protectionOverlay) {
            return { mode: "NONE" };
        }
        return siteState.protectionOverlay;
    }

    function isProtectionOverlayActive(state) {
        const protectionOverlay = getProtectionOverlayState(state);
        return !!protectionOverlay && protectionOverlay.mode && protectionOverlay.mode !== "NONE";
    }

    function getActiveSiteName(state) {
        const siteBootstrap = getSiteBootstrap(state);
        if (siteBootstrap && typeof siteBootstrap.siteId === "number") {
            return "Site " + siteBootstrap.siteId;
        }

        const siteState = getSiteState(state);
        if (siteState && typeof siteState.siteId === "number") {
            return "Site " + siteState.siteId;
        }

        return "Active Site";
    }

    function getActiveSiteTip() {
        const tipIndex = Math.floor(performance.now() / 20000) % siteTutorialTips.length;
        return siteTutorialTips[tipIndex];
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
                flags: typeof storage.flags === "number" ? storage.flags : 0,
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
                    flags: 0,
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

    function getCampaignResources(state) {
        return state ? state.campaignResources || null : null;
    }

    function createAudioManagerState() {
        return {
            enabled: false,
            context: null,
            masterGain: null,
            ambienceBus: null,
            musicBus: null,
            cuesBus: null,
            noiseBuffer: null,
            windLayer: null,
            gritLayer: null,
            menuPadLow: null,
            menuPadHigh: null,
            oasisPadLow: null,
            oasisPadHigh: null,
            tensionDroneLow: null,
            tensionDroneHigh: null
        };
    }

    function updateAudioToggleButton() {
        if (!audioToggle) {
            return;
        }

        let label = "Audio Off";
        if (audioManager.enabled) {
            if (audioManager.context && audioManager.context.state === "running") {
                label = "Audio On";
            } else {
                label = "Audio Tap To Resume";
            }
        }

        audioToggle.textContent = label;
        audioToggle.setAttribute("data-audio-enabled", audioManager.enabled ? "true" : "false");
    }

    function createAudioNoiseBuffer(audioContext) {
        const sampleRate = audioContext.sampleRate;
        const durationSeconds = 2.0;
        const frameCount = Math.max(1, Math.floor(sampleRate * durationSeconds));
        const buffer = audioContext.createBuffer(1, frameCount, sampleRate);
        const channelData = buffer.getChannelData(0);
        for (let index = 0; index < frameCount; index += 1) {
            channelData[index] = Math.random() * 2.0 - 1.0;
        }
        return buffer;
    }

    function createNoiseLayer(audioContext, noiseBuffer, filterType, filterFrequency, qValue, destination) {
        const source = audioContext.createBufferSource();
        source.buffer = noiseBuffer;
        source.loop = true;

        const filter = audioContext.createBiquadFilter();
        filter.type = filterType;
        filter.frequency.value = filterFrequency;
        filter.Q.value = qValue;

        const gain = audioContext.createGain();
        gain.gain.value = 0.0;

        source.connect(filter);
        filter.connect(gain);
        gain.connect(destination);
        source.start();

        return {
            source: source,
            filter: filter,
            gain: gain
        };
    }

    function createOscillatorLayer(audioContext, type, frequency, destination) {
        const oscillator = audioContext.createOscillator();
        oscillator.type = type;
        oscillator.frequency.value = frequency;

        const gain = audioContext.createGain();
        gain.gain.value = 0.0;

        oscillator.connect(gain);
        gain.connect(destination);
        oscillator.start();

        return {
            oscillator: oscillator,
            gain: gain
        };
    }

    function ensureAudioGraph() {
        const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
        if (!AudioContextCtor) {
            if (audioToggle) {
                audioToggle.disabled = true;
                audioToggle.textContent = "Audio Unsupported";
            }
            return null;
        }

        if (audioManager.context) {
            return audioManager.context;
        }

        const audioContext = new AudioContextCtor();
        const masterGain = audioContext.createGain();
        const ambienceBus = audioContext.createGain();
        const musicBus = audioContext.createGain();
        const cuesBus = audioContext.createGain();
        masterGain.gain.value = 0.0;
        ambienceBus.gain.value = 0.9;
        musicBus.gain.value = 0.55;
        cuesBus.gain.value = 0.9;
        ambienceBus.connect(masterGain);
        musicBus.connect(masterGain);
        cuesBus.connect(masterGain);
        masterGain.connect(audioContext.destination);

        const noiseBuffer = createAudioNoiseBuffer(audioContext);

        audioManager.context = audioContext;
        audioManager.masterGain = masterGain;
        audioManager.ambienceBus = ambienceBus;
        audioManager.musicBus = musicBus;
        audioManager.cuesBus = cuesBus;
        audioManager.noiseBuffer = noiseBuffer;
        audioManager.windLayer = createNoiseLayer(audioContext, noiseBuffer, "bandpass", 420.0, 0.6, ambienceBus);
        audioManager.gritLayer = createNoiseLayer(audioContext, noiseBuffer, "highpass", 1800.0, 0.25, ambienceBus);
        audioManager.menuPadLow = createOscillatorLayer(audioContext, "sine", 196.0, musicBus);
        audioManager.menuPadHigh = createOscillatorLayer(audioContext, "triangle", 293.66, musicBus);
        audioManager.oasisPadLow = createOscillatorLayer(audioContext, "sine", 261.63, musicBus);
        audioManager.oasisPadHigh = createOscillatorLayer(audioContext, "triangle", 392.0, musicBus);
        audioManager.tensionDroneLow = createOscillatorLayer(audioContext, "sawtooth", 55.0, musicBus);
        audioManager.tensionDroneHigh = createOscillatorLayer(audioContext, "triangle", 82.41, musicBus);
        updateAudioToggleButton();
        return audioContext;
    }

    function resumeAudioGraphIfNeeded() {
        const audioContext = ensureAudioGraph();
        if (!audioContext) {
            return Promise.resolve(false);
        }

        if (audioContext.state === "running") {
            updateAudioToggleButton();
            return Promise.resolve(true);
        }

        return audioContext.resume()
            .then(function () {
                updateAudioToggleButton();
                return true;
            })
            .catch(function () {
                updateAudioToggleButton();
                return false;
            });
    }

    function getApproximateLocalPlantDensity(state) {
        const siteBootstrap = getSiteBootstrap(state);
        const siteState = getSiteState(state);
        const worker = siteState ? siteState.worker : null;
        const tiles = siteBootstrap && Array.isArray(siteBootstrap.tiles) ? siteBootstrap.tiles : null;
        if (!worker || !tiles || tiles.length === 0) {
            return 0.0;
        }

        const workerTileX = typeof worker.tileX === "number" ? worker.tileX : 0.0;
        const workerTileY = typeof worker.tileY === "number" ? worker.tileY : 0.0;
        let totalWeight = 0.0;
        let weightedDensity = 0.0;

        tiles.forEach((tile) => {
            if (!tile || typeof tile.x !== "number" || typeof tile.y !== "number") {
                return;
            }
            const dx = tile.x - workerTileX;
            const dy = tile.y - workerTileY;
            const distance = Math.hypot(dx, dy);
            if (distance > 2.6) {
                return;
            }

            const weight = 1.0 / (1.0 + distance);
            const density = clamp01(typeof tile.plantDensity === "number" ? tile.plantDensity : 0.0);
            totalWeight += weight;
            weightedDensity += density * weight;
        });

        if (!(totalWeight > 0.0)) {
            return 0.0;
        }

        return clamp01(weightedDensity / totalWeight);
    }

    function getAudioHazardLevel(state) {
        const siteState = getSiteState(state);
        const weather = siteState ? siteState.weather : null;
        const hud = getHudState(state);
        const warningCode = hud && typeof hud.warningCode === "number" ? hud.warningCode : 0;
        const warningHazard = Object.prototype.hasOwnProperty.call(audioWarningHazardLevels, warningCode)
            ? audioWarningHazardLevels[warningCode]
            : 0.0;
        const windHazard = weather ? clamp01((weather.wind || 0) / 10.0) : 0.0;
        const dustHazard = weather ? clamp01((weather.dust || 0) / 100.0) : 0.0;
        const eventHazard = weather && weather.eventTemplateId ? 0.18 : 0.0;
        return clamp01(Math.max(warningHazard, windHazard, dustHazard, eventHazard));
    }

    function buildAudioMixState(state) {
        const mix = {
            master: 0.0,
            wind: 0.0,
            windFilterHz: 420.0,
            grit: 0.0,
            gritFilterHz: 1800.0,
            menuPad: 0.0,
            oasisPad: 0.0,
            tensionPad: 0.0
        };

        if (!state || !state.appState) {
            return mix;
        }

        if (state.appState === "MAIN_MENU") {
            mix.master = 0.48;
            mix.wind = 0.016;
            mix.windFilterHz = 320.0;
            mix.menuPad = 0.072;
            return mix;
        }

        if (state.appState === "REGIONAL_MAP") {
            mix.master = 0.52;
            mix.menuPad = 0.048;
            mix.oasisPad = 0.028;
            return mix;
        }

        if (state.appState === "SITE_RESULT") {
            const siteResult = getSiteResult(state);
            mix.master = 0.58;
            if (siteResult && siteResult.result === "COMPLETED") {
                mix.wind = 0.014;
                mix.oasisPad = 0.11;
            } else {
                mix.wind = 0.038;
                mix.grit = 0.032;
                mix.tensionPad = 0.09;
            }
            return mix;
        }

        if (state.appState !== "SITE_ACTIVE") {
            return mix;
        }

        const weatherVisualResponse = getWeatherVisualResponse();
        const localDensity = getApproximateLocalPlantDensity(state);
        const hazardLevel = getAudioHazardLevel(state);
        const shelteredFactor = clamp01(localDensity * 1.1 - hazardLevel * 0.35);
        mix.master = 0.62;
        mix.wind = 0.018 + weatherVisualResponse.windLevel * 0.14 + (1.0 - shelteredFactor) * 0.035;
        mix.windFilterHz = 240.0 + weatherVisualResponse.windLevel * 520.0 - shelteredFactor * 80.0;
        mix.grit = weatherVisualResponse.dustLevel * 0.11 + hazardLevel * 0.09;
        mix.gritFilterHz = 1400.0 + hazardLevel * 1800.0;
        mix.oasisPad = shelteredFactor * (1.0 - hazardLevel) * 0.095;
        mix.tensionPad = Math.max(0.0, hazardLevel - 0.18) * 0.18;
        return mix;
    }

    function setAudioParamTarget(audioParam, value, timeConstant) {
        if (!audioParam) {
            return;
        }
        const audioContext = audioManager.context;
        if (!audioContext) {
            return;
        }
        audioParam.setTargetAtTime(value, audioContext.currentTime, timeConstant);
    }

    function updateAudioMix(state) {
        if (!audioManager.enabled) {
            if (audioManager.masterGain && audioManager.context) {
                setAudioParamTarget(audioManager.masterGain.gain, 0.0, 0.05);
            }
            updateAudioToggleButton();
            return;
        }

        const audioContext = ensureAudioGraph();
        if (!audioContext) {
            return;
        }

        if (audioContext.state !== "running") {
            updateAudioToggleButton();
            return;
        }

        const mix = buildAudioMixState(state);
        setAudioParamTarget(audioManager.masterGain.gain, mix.master, 0.18);
        setAudioParamTarget(audioManager.windLayer.gain.gain, mix.wind, 0.22);
        setAudioParamTarget(audioManager.windLayer.filter.frequency, mix.windFilterHz, 0.24);
        setAudioParamTarget(audioManager.gritLayer.gain.gain, mix.grit, 0.14);
        setAudioParamTarget(audioManager.gritLayer.filter.frequency, mix.gritFilterHz, 0.18);
        setAudioParamTarget(audioManager.menuPadLow.gain.gain, mix.menuPad * 0.72, 0.32);
        setAudioParamTarget(audioManager.menuPadHigh.gain.gain, mix.menuPad * 0.54, 0.32);
        setAudioParamTarget(audioManager.oasisPadLow.gain.gain, mix.oasisPad * 0.65, 0.28);
        setAudioParamTarget(audioManager.oasisPadHigh.gain.gain, mix.oasisPad * 0.42, 0.28);
        setAudioParamTarget(audioManager.tensionDroneLow.gain.gain, mix.tensionPad * 0.7, 0.16);
        setAudioParamTarget(audioManager.tensionDroneHigh.gain.gain, mix.tensionPad * 0.26, 0.16);
        updateAudioToggleButton();
    }

    function scheduleToneBurst(options) {
        const audioContext = audioManager.context;
        if (!audioContext || !audioManager.cuesBus || audioContext.state !== "running") {
            return;
        }

        const now = audioContext.currentTime;
        const oscillator = audioContext.createOscillator();
        const gain = audioContext.createGain();
        oscillator.type = options.type || "sine";
        oscillator.frequency.setValueAtTime(options.startFrequency || 440.0, now);
        if (typeof options.endFrequency === "number") {
            oscillator.frequency.exponentialRampToValueAtTime(
                Math.max(20.0, options.endFrequency),
                now + Math.max(0.02, options.duration || 0.18));
        }

        gain.gain.setValueAtTime(0.0001, now);
        gain.gain.exponentialRampToValueAtTime(Math.max(0.0002, options.peakGain || 0.06), now + 0.02);
        gain.gain.exponentialRampToValueAtTime(0.0001, now + Math.max(0.06, options.duration || 0.18));
        oscillator.connect(gain);
        gain.connect(audioManager.cuesBus);
        oscillator.start(now);
        oscillator.stop(now + Math.max(0.08, options.duration || 0.18) + 0.04);
        oscillator.onended = function () {
            oscillator.disconnect();
            gain.disconnect();
        };
    }

    function scheduleNoiseBurst(options) {
        const audioContext = audioManager.context;
        if (!audioContext || !audioManager.cuesBus || !audioManager.noiseBuffer || audioContext.state !== "running") {
            return;
        }

        const now = audioContext.currentTime;
        const source = audioContext.createBufferSource();
        source.buffer = audioManager.noiseBuffer;

        const filter = audioContext.createBiquadFilter();
        filter.type = options.filterType || "highpass";
        filter.frequency.value = options.filterFrequency || 1600.0;
        filter.Q.value = options.qValue || 0.3;

        const gain = audioContext.createGain();
        gain.gain.setValueAtTime(0.0001, now);
        gain.gain.exponentialRampToValueAtTime(Math.max(0.0002, options.peakGain || 0.05), now + 0.015);
        gain.gain.exponentialRampToValueAtTime(0.0001, now + Math.max(0.05, options.duration || 0.16));

        source.connect(filter);
        filter.connect(gain);
        gain.connect(audioManager.cuesBus);
        source.start(now);
        source.stop(now + Math.max(0.08, options.duration || 0.16) + 0.03);
        source.onended = function () {
            source.disconnect();
            filter.disconnect();
            gain.disconnect();
        };
    }

    function playOneShotCue(cue) {
        if (!audioManager.enabled || !cue || !cue.cueKind) {
            return;
        }

        switch (cue.cueKind) {
        case "ACTION_COMPLETED":
            scheduleToneBurst({ type: "triangle", startFrequency: 460.0, endFrequency: 620.0, duration: 0.18, peakGain: 0.035 });
            scheduleToneBurst({ type: "sine", startFrequency: 620.0, endFrequency: 780.0, duration: 0.14, peakGain: 0.022 });
            break;
        case "ACTION_FAILED":
            scheduleToneBurst({ type: "square", startFrequency: 280.0, endFrequency: 150.0, duration: 0.2, peakGain: 0.045 });
            break;
        case "HAZARD_PEAK":
            scheduleNoiseBurst({ filterType: "highpass", filterFrequency: 2100.0, duration: 0.3, peakGain: 0.06 });
            scheduleToneBurst({ type: "sawtooth", startFrequency: 120.0, endFrequency: 68.0, duration: 0.36, peakGain: 0.04 });
            break;
        case "SITE_COMPLETED":
            scheduleToneBurst({ type: "sine", startFrequency: 392.0, endFrequency: 523.25, duration: 0.45, peakGain: 0.04 });
            scheduleToneBurst({ type: "triangle", startFrequency: 523.25, endFrequency: 659.25, duration: 0.52, peakGain: 0.028 });
            break;
        case "SITE_FAILED":
            scheduleToneBurst({ type: "sawtooth", startFrequency: 164.81, endFrequency: 82.41, duration: 0.55, peakGain: 0.05 });
            scheduleNoiseBurst({ filterType: "bandpass", filterFrequency: 420.0, duration: 0.24, peakGain: 0.03, qValue: 0.7 });
            break;
        default:
            break;
        }
    }

    function processRecentOneShotCues(state) {
        const recentOneShotCues = state && Array.isArray(state.recentOneShotCues)
            ? state.recentOneShotCues
            : [];
        recentOneShotCues.forEach((cue) => {
            const sequenceId = typeof cue.sequenceId === "number" ? cue.sequenceId : -1;
            if (sequenceId <= lastProcessedOneShotCueSequenceId) {
                return;
            }
            lastProcessedOneShotCueSequenceId = sequenceId;
            playOneShotCue(cue);
        });
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
        return itemVisuals[itemId] || { iconKey: "idle", light: "#8f7a64", dark: "#5a4837" };
    }

    function createUiIcon(iconKey, extraClassName) {
        const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        svg.setAttribute("viewBox", "0 0 24 24");
        svg.setAttribute("fill", "none");
        svg.setAttribute("stroke", "currentColor");
        svg.setAttribute("stroke-width", "1.9");
        svg.setAttribute("stroke-linecap", "round");
        svg.setAttribute("stroke-linejoin", "round");
        svg.setAttribute("aria-hidden", "true");
        svg.classList.add("ui-icon-svg");
        if (extraClassName) {
            extraClassName.split(/\s+/).filter(Boolean).forEach((className) => svg.classList.add(className));
        }
        svg.innerHTML = uiIconMarkup[iconKey] || uiIconMarkup.idle;
        return svg;
    }

    function appendUiIcon(container, iconKey, extraClassName) {
        if (!container) {
            return;
        }
        container.textContent = "";
        container.appendChild(createUiIcon(iconKey, extraClassName));
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

    function getHarvestablePlantMeta(plantTypeId) {
        return harvestablePlantCatalog[plantTypeId] || null;
    }

    function getHarvestOptionForTile(state, tileX, tileY) {
        const tile = getTileSnapshot(state, tileX, tileY);
        if (!tile || !tile.plantTypeId) {
            return null;
        }

        const harvestMeta = getHarvestablePlantMeta(tile.plantTypeId);
        if (!harvestMeta) {
            return null;
        }

        const itemMeta = getItemMeta(harvestMeta.itemId);
        const visual = getItemVisual(harvestMeta.itemId);
        const density = typeof tile.plantDensity === "number" ? tile.plantDensity : 0;
        return {
            plantTypeId: tile.plantTypeId || 0,
            itemId: harvestMeta.itemId,
            itemLabel: itemMeta ? itemMeta.name : ("Item " + harvestMeta.itemId),
            shortLabel: itemMeta ? itemMeta.shortName : ("Item " + harvestMeta.itemId),
            iconKey: visual.iconKey,
            iconLight: visual.light,
            iconDark: visual.dark,
            density: density,
            lootDensityThreshold: harvestMeta.lootDensityThreshold,
            yieldsItem: density > (harvestMeta.lootDensityThreshold + 0.0001)
        };
    }

    function getVisibleExcavationDepth(tile) {
        return tile && typeof tile.visibleExcavationDepth === "number"
            ? Math.max(0, Math.min(3, tile.visibleExcavationDepth | 0))
            : 0;
    }

    function excavationDepthLabel(depth) {
        if (depth === 1) {
            return "Rough";
        }
        if (depth === 2) {
            return "Careful";
        }
        if (depth === 3) {
            return "Thorough";
        }
        return "None";
    }

    function getExcavationOptionForTile(state, tileX, tileY) {
        const tile = getTileSnapshot(state, tileX, tileY);
        if (!tile) {
            return null;
        }

        const occupied =
            !!(tile.plantTypeId || 0) ||
            !!(tile.structureTypeId || 0) ||
            !!(tile.groundCoverTypeId || 0);
        const activeExcavationTile = getActiveExcavationTile(getSiteAction(state));
        const tileAlreadyReservedForExcavation =
            !!activeExcavationTile &&
            activeExcavationTile.x === tileX &&
            activeExcavationTile.y === tileY;
        const visibleDepth = getVisibleExcavationDepth(tile);
        const storedDepth =
            tile && typeof tile.excavationDepth === "number"
                ? Math.max(0, Math.min(3, tile.excavationDepth | 0))
                : 0;
        if (occupied || visibleDepth >= 1 || storedDepth >= 1 || tileAlreadyReservedForExcavation) {
            return null;
        }

        return {
            requestedDepth: 1,
            label: "Rough Excavation",
            shortLabel: "rough excavation",
            meta: "30% base find chance. Deeper passes unlock later.",
            iconKey: "stone",
            iconLight: "#b89c6d",
            iconDark: "#65523a"
        };
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

    function getPhonePanelState(state) {
        const siteState = getSiteState(state);
        if (!siteState || !siteState.phonePanel) {
            return null;
        }

        return siteState.phonePanel;
    }

    function isPhonePanelOpen(state) {
        const phonePanelState = getPhonePanelState(state);
        return !!(phonePanelState && phonePanelState.open);
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

    function getPlacementPreview(state) {
        const siteState = getSiteState(state);
        return siteState ? (siteState.placementPreview || null) : null;
    }

    function isPlacementModeActive(state) {
        const preview = getPlacementPreview(state);
        return !!(preview && (preview.flags & 1) !== 0 && preview.actionKind !== 0);
    }

    function placementPreviewIsValid(preview) {
        return !!(preview && (preview.flags & 2) !== 0);
    }

    function placementActionKindName(actionKind) {
        if (actionKind === 1) {
            return "PLANT";
        }
        if (actionKind === 2) {
            return "BUILD";
        }
        return "";
    }

    function placementModeLabel(preview) {
        if (!preview) {
            return "Placement";
        }

        if (preview.itemId) {
            const itemMeta = getItemMeta(preview.itemId);
            if (itemMeta) {
                return itemMeta.shortName || itemMeta.name;
            }
        }

        return getSiteActionLabel(preview.actionKind || 0);
    }

    function renderPlacementFailureToast() {
        if (!placementToast) {
            return;
        }

        if (!placementFailureToastState || performance.now() >= placementFailureToastState.expiresAtMs) {
            placementToast.hidden = true;
            return;
        }

        placementToast.hidden = false;
        placementToastBody.textContent = placementFailureToastState.message;
    }

    function syncPlacementFailureToast(state) {
        const siteState = getSiteState(state);
        const failure = siteState ? (siteState.placementFailure || null) : null;
        if (!failure || !failure.sequenceId) {
            return;
        }

        if (placementFailureToastState &&
            placementFailureToastState.sequenceId === failure.sequenceId) {
            return;
        }

        const actionLabel = getSiteActionLabel(failure.actionKind || 0);
        placementFailureToastState = {
            sequenceId: failure.sequenceId,
            expiresAtMs: performance.now() + 1800,
            message: actionLabel === "Idle"
                ? "This placement is blocked."
                : (actionLabel + " placement is blocked.")
        };
        renderPlacementFailureToast();
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
            flags: containerInfo.flags || 0,
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
        const siteState = getSiteState(state);
        const openedStorage =
            siteState && siteState.openedStorage && typeof siteState.openedStorage.storageId === "number"
                ? siteState.openedStorage
                : null;
        if (!openedStorage) {
            return null;
        }

        openedInventoryContainerKey = containerKey("DEVICE_STORAGE", openedStorage.storageId);
        return findInventoryContainerByKey(openedInventoryContainerKey);
    }

    function clearOpenedInventoryContainerIfInvalid(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            openedInventoryContainerKey = "";
            return;
        }

        const siteState = getSiteState(state);
        const openedStorage =
            siteState && siteState.openedStorage && typeof siteState.openedStorage.storageId === "number"
                ? siteState.openedStorage
                : null;
        if (!openedStorage) {
            openedInventoryContainerKey = "";
            return;
        }

        if (!findInventoryContainerByKey(containerKey("DEVICE_STORAGE", openedStorage.storageId))) {
            openedInventoryContainerKey = "";
        }
    }

    function openInventoryContainer(containerInfo) {
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
        const closedContainerKey = openedContainer ? openedContainer.key : openedInventoryContainerKey;
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
        if ((containerInfo.flags & inventoryStorageFlags.deliveryBox) !== 0) {
            return "Delivery Crate (" + containerInfo.containerTileX + ", " + containerInfo.containerTileY + ")";
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
                    iconKey: visual.iconKey,
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
                    iconKey: visual.iconKey,
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

    function isWorkerPackPanelOpen(state) {
        const siteState = getSiteState(state);
        return !!(siteState && siteState.workerPackOpen);
    }

    function postWorkerPackPanelEvent(eventKind) {
        const workerPackContainer = latestState ? findWorkerPackInventoryContainer(latestState) : null;
        if (!workerPackContainer || typeof workerPackContainer.storageId !== "number") {
            return Promise.reject(new Error("Worker pack storage is unavailable."));
        }

        return postJson("/site-storage-view", {
            storageId: workerPackContainer.storageId,
            eventKind: eventKind
        });
    }

    function slotUsesPrimaryTransferClick(state, sourceSlot) {
        if (!state || state.appState !== "SITE_ACTIVE" || !isOccupiedSlot(sourceSlot)) {
            return false;
        }

        if (sourceSlot.containerKind === "WORKER_PACK") {
            const openedContainer = findOpenedInventoryContainer(state);
            return !!(
                openedContainer &&
                openedContainer.containerKind === "DEVICE_STORAGE" &&
                (openedContainer.flags & inventoryStorageFlags.retrievalOnly) === 0
            );
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

    function postSiteActionCancel(payload) {
        return postJson("/site-action-cancel", payload);
    }

    function postSiteContextRequest(tileX, tileY) {
        return postJson("/site-context", {
            tileX: tileX,
            tileY: tileY,
            flags: 0
        });
    }

    function cancelPlacementMode() {
        if (!isPlacementModeActive(latestState)) {
            return Promise.resolve();
        }

        return postSiteActionCancel({
            actionId: 0,
            flags: siteActionCancelFlags.placementMode
        }).catch(() => {
            statusChip.textContent = "Failed to cancel placement mode.";
        });
    }

    function cancelProtectionOverlay() {
        if (!isProtectionOverlayActive(latestState)) {
            return Promise.resolve();
        }

        return postCloseSiteProtectionUi().catch(() => {
            statusChip.textContent = "Failed to close protection overlay.";
        });
    }

    function sendPlacementCursorUpdate(tileX, tileY) {
        if (!isPlacementModeActive(latestState)) {
            lastSentPlacementCursorSignature = "";
            return;
        }

        const signature = String(tileX) + ":" + String(tileY);
        if (placementCursorSendInFlight || signature === lastSentPlacementCursorSignature) {
            return;
        }

        placementCursorSendInFlight = true;
        postSiteContextRequest(tileX, tileY)
            .then(() => {
                lastSentPlacementCursorSignature = signature;
            })
            .catch(() => {})
            .finally(() => {
                placementCursorSendInFlight = false;
            });
    }

    function confirmPlacementModeAtTile(tileData) {
        const preview = getPlacementPreview(latestState);
        if (!tileData || !preview) {
            return;
        }

        const actionKind = placementActionKindName(preview.actionKind || 0);
        if (!actionKind) {
            return;
        }

        statusChip.textContent =
            placementPreviewIsValid(preview)
                ? ("Confirming " + placementModeLabel(preview) + " at (" + tileData.tileX + "," + tileData.tileY + ").")
                : ("Blocked placement at (" + tileData.tileX + "," + tileData.tileY + ").");

        postSiteAction({
            actionKind: actionKind,
            flags: preview.itemId ? siteActionRequestFlags.hasItem : 0,
            quantity: 1,
            targetTileX: tileData.tileX,
            targetTileY: tileData.tileY,
            primarySubjectId: 0,
            secondarySubjectId: 0,
            itemId: preview.itemId || 0
        }).catch(() => {
            statusChip.textContent = "Failed to confirm placement action.";
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
        if (actionKind === 9) {
            return "Harvest";
        }
        if (actionKind === 10) {
            return "Excavate";
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
        const placementPreview = getPlacementPreview(state);
        const workerActionKind =
            siteState && siteState.worker ? siteState.worker.currentActionKind : 0;
        const warning = getHudWarningPresentation(state);
        const hydration = hud ? Math.round(hud.playerHydration) : 0;
        const energy = hud ? Math.round(hud.playerEnergy) : 0;
        const completion = hud ? Math.round((hud.siteCompletionNormalized || 0) * 100) : 0;
        const weatherHeat = weather ? Math.round(weather.heat || 0) : 0;
        const weatherWind = weather ? Math.round(weather.wind || 0) : 0;
        const weatherDust = weather ? Math.round(weather.dust || 0) : 0;
        const windDirection = weather ? Math.round(weather.windDirectionDegrees || 0) : 0;
        const eventTemplateId = weather ? Math.round(weather.eventTemplateId || 0) : 0;
        const eventStartTime = weather ? Math.round(weather.eventStartTimeMinutes || 0) : 0;
        const eventPeakTime = weather ? Math.round(weather.eventPeakTimeMinutes || 0) : 0;
        const eventPeakDuration = weather ? Math.round(weather.eventPeakDurationMinutes || 0) : 0;
        const eventEndTime = weather ? Math.round(weather.eventEndTimeMinutes || 0) : 0;

        statusChip.textContent =
            "Site Live\nHydration " + hydration +
            "\nEnergy " + energy +
            "\nCompletion " + completion + "%" +
            "\nHeat " + weatherHeat +
            "\nWind " + weatherWind +
            "\nBearing " + windDirection + "deg" +
            "\nDust " + weatherDust +
            "\nEvent " + eventTemplateId +
            "\nTimeline " + eventStartTime + "-" + eventPeakTime + "+" + eventPeakDuration + "-" + eventEndTime +
            "\nAlert " + warning.headline +
            "\nSeeds " + carriedSeeds.reduce((total, seed) => total + seed.quantity, 0) +
            "\nAction " + getSiteActionLabel(workerActionKind) +
            "\nMode " + (isPlacementModeActive(state) ? placementModeLabel(placementPreview) : "None");
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
        tileContextMenuRenderSignature = "";
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
        const harvestOption = getHarvestOptionForTile(state, tileX, tileY);
        const excavationOption = getExcavationOptionForTile(state, tileX, tileY);
        const rootItems = [];
        const tileHasStructure = structureId !== 0;

        if (storageContainers.length === 1) {
            const containerInfo = storageContainers[0];
            rootItems.push({
                id: "open-storage:" + containerInfo.key,
                label: "Open Storage",
                meta: getContainerDisplayName(state, containerInfo),
                iconKey: "box",
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
                iconKey: "box",
                iconLight: "#8f7a64",
                iconDark: "#5a4837",
                children: storageContainers.map((containerInfo) => ({
                    id: "open-storage:" + containerInfo.key,
                    label: getContainerDisplayName(state, containerInfo),
                    meta: "Open this container",
                    iconKey: "box",
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
                iconKey: "box",
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
                    iconKey: "craft",
                    iconLight: "#7a9d67",
                    iconDark: "#4a6b3b",
                    disabled: true
                });
            } else if (Array.isArray(craftContext.options) && craftContext.options.length > 0) {
            rootItems.push({
                id: "craft",
                label: "Craft",
                meta: "Choose one recipe.",
                iconKey: "craft",
                iconLight: "#7a9d67",
                iconDark: "#4a6b3b",
                    children: craftContext.options.map((option) => {
                    const itemMeta = getItemMeta(option.outputItemId);
                    const visual = getItemVisual(option.outputItemId);
                    return {
                        id: "craft:" + option.outputItemId,
                        label: itemMeta ? itemMeta.name : ("Item " + option.outputItemId),
                        meta: "Ready",
                        iconKey: visual.iconKey,
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
                iconKey: "hammer",
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

        if (harvestOption) {
            rootItems.push({
                id: "harvest",
                label: "Harvest",
                meta: harvestOption.yieldsItem
                    ? ("Loot: " + harvestOption.itemLabel)
                    : ("No loot at " + Math.round(harvestOption.density * 100) + "% density; this cut will only reduce or remove the plant"),
                iconKey: harvestOption.iconKey,
                iconLight: harvestOption.iconLight,
                iconDark: harvestOption.iconDark,
                onSelect: function () {
                    statusChip.textContent =
                        "Moving to (" + tileX + "," + tileY + ") to harvest " +
                        harvestOption.shortLabel + ".";
                    postSiteAction({
                        actionKind: "HARVEST",
                        flags: 0,
                        quantity: 1,
                        targetTileX: tileX,
                        targetTileY: tileY,
                        primarySubjectId: 0,
                        secondarySubjectId: 0,
                        itemId: 0
                    }).catch(() => {
                        statusChip.textContent = "Failed to send harvest action.";
                    });
                    closeTileContextMenu();
                }
            });
        }

        if (excavationOption) {
            rootItems.push({
                id: "excavate",
                label: "Excavate",
                meta: excavationOption.meta,
                iconKey: excavationOption.iconKey,
                iconLight: excavationOption.iconLight,
                iconDark: excavationOption.iconDark,
                onSelect: function () {
                    statusChip.textContent =
                        "Moving to (" + tileX + "," + tileY + ") for " +
                        excavationOption.shortLabel + ".";
                    postSiteAction({
                        actionKind: "EXCAVATE",
                        flags: 0,
                        quantity: 1,
                        targetTileX: tileX,
                        targetTileY: tileY,
                        primarySubjectId: 0,
                        secondarySubjectId: excavationOption.requestedDepth,
                        itemId: 0
                    }).catch(() => {
                        statusChip.textContent = "Failed to send excavation action.";
                    });
                    closeTileContextMenu();
                }
            });
        }

        if (carriedSeeds.length > 0) {
            rootItems.push({
                id: "plant",
                label: "Plant",
                meta: "Choose one carried seed type for placement mode.",
                iconKey: "sprout",
                iconLight: "#7a9d67",
                iconDark: "#4a6b3b",
                children: carriedSeeds.map((seed) => {
                    return {
                        id: seed.id,
                        label: seed.label,
                        meta: "x" + seed.quantity + " carried",
                        iconKey: seed.iconKey,
                        iconLight: seed.iconLight,
                        iconDark: seed.iconDark,
                        onSelect: function () {
                            statusChip.textContent =
                                "Placement mode: " + seed.shortLabel + ". Left-click to confirm, Esc or right-click to cancel.";
                            postSiteAction({
                                actionKind: "PLANT",
                                flags: siteActionRequestFlags.hasItem | siteActionRequestFlags.deferredTargetSelection,
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

        if (carriedDeployables.length > 0) {
            rootItems.push({
                id: "deploy",
                label: "Deploy",
                meta: "Choose one carried device kit for placement mode.",
                iconKey: "camp",
                iconLight: "#b48857",
                iconDark: "#6a4b2e",
                children: carriedDeployables.map((item) => {
                    return {
                        id: item.id,
                        label: item.label,
                        meta: "x" + item.quantity + " carried",
                        iconKey: item.iconKey,
                        iconLight: item.iconLight,
                        iconDark: item.iconDark,
                        onSelect: function () {
                            statusChip.textContent =
                                "Placement mode: " + item.shortLabel + ". Left-click to confirm, Esc or right-click to cancel.";
                            postSiteAction({
                                actionKind: "BUILD",
                                flags: siteActionRequestFlags.hasItem | siteActionRequestFlags.deferredTargetSelection,
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
                    iconKey: "idle",
                    iconLight: "#a38e76",
                    iconDark: "#796652",
                    disabled: true
                }
            ];
        }

        return rootItems;
    }

    function summarizeContextMenuTree(items) {
        if (!Array.isArray(items)) {
            return [];
        }

        return items.map((item) => ({
            id: item.id || "",
            label: item.label || "",
            meta: item.meta || "",
            disabled: !!item.disabled,
            children: summarizeContextMenuTree(item.children || [])
        }));
    }

    function renderTileContextMenu() {
        if (!tileContextMenuState ||
            !latestState ||
            latestState.appState !== "SITE_ACTIVE" ||
            isPlacementModeActive(latestState)) {
            closeTileContextMenu();
            return;
        }

        const rootItems = buildTileContextMenuItems(
            latestState,
            tileContextMenuState.tileX,
            tileContextMenuState.tileY);
        const renderSignature = JSON.stringify({
            tileX: tileContextMenuState.tileX,
            tileY: tileContextMenuState.tileY,
            anchorX: tileContextMenuState.anchorX,
            anchorY: tileContextMenuState.anchorY,
            hoverPath: tileContextMenuState.hoverPath,
            items: summarizeContextMenuTree(rootItems)
        });
        if (renderSignature === tileContextMenuRenderSignature) {
            return;
        }
        tileContextMenuRenderSignature = renderSignature;
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
                appendUiIcon(icon, item.iconKey || "idle");
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
        if (!latestState || latestState.appState !== "SITE_ACTIVE" || isPlacementModeActive(latestState)) {
            return;
        }

        tileContextMenuState = {
            tileX: tileData && typeof tileData.tileX === "number" ? tileData.tileX : 0,
            tileY: tileData && typeof tileData.tileY === "number" ? tileData.tileY : 0,
            anchorX: clientX,
            anchorY: clientY,
            hoverPath: []
        };
        tileContextMenuRenderSignature = "";
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

    function hideModifierTooltip() {
        if (modifierTooltip) {
            modifierTooltip.hidden = true;
        }
    }

    function hideTechTooltip() {
        if (techTooltip) {
            techTooltip.hidden = true;
        }
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

    function moveModifierTooltip(clientX, clientY) {
        if (!modifierTooltip) {
            return;
        }

        const tooltipWidth = 260;
        const tooltipHeight = 120;
        const left = Math.min(clientX + 18, Math.max(12, window.innerWidth - tooltipWidth - 16));
        const top = Math.min(clientY + 18, Math.max(12, window.innerHeight - tooltipHeight - 16));
        modifierTooltip.style.left = left + "px";
        modifierTooltip.style.top = top + "px";
    }

    function moveTechTooltip(clientX, clientY) {
        if (!techTooltip) {
            return;
        }

        const tooltipWidth = 300;
        const tooltipHeight = 132;
        const left = Math.min(clientX + 18, Math.max(12, window.innerWidth - tooltipWidth - 16));
        const top = Math.min(clientY + 18, Math.max(12, window.innerHeight - tooltipHeight - 16));
        techTooltip.style.left = left + "px";
        techTooltip.style.top = top + "px";
    }

    function showModifierTooltip(modifier, clientX, clientY) {
        if (!modifierTooltip || !modifier) {
            hideModifierTooltip();
            return;
        }

        modifierTooltipTitle.textContent = modifier.name || "Modifier";
        modifierTooltipMeta.textContent = buildModifierTooltipMeta(modifier);
        modifierTooltip.hidden = false;
        moveModifierTooltip(clientX, clientY);
    }

    function formatTechNodeKindLabel(nodeModel) {
        if (!nodeModel) {
            return "Tech";
        }
        if (nodeModel.isEnhancement) {
            return nodeModel.enhancementChoiceIndex > 0
                ? ("Enhancement " + nodeModel.enhancementChoiceIndex)
                : "Enhancement";
        }
        return "Base Tech";
    }

    function buildTechTooltipMeta(nodeModel) {
        if (!nodeModel) {
            return "No details.";
        }

        const lines = [];
        if (nodeModel.descriptionText) {
            lines.push(nodeModel.descriptionText);
        }

        const identityParts = [];
        if (nodeModel.factionText) {
            identityParts.push(nodeModel.factionText);
        }
        identityParts.push(formatTechNodeKindLabel(nodeModel));
        if (nodeModel.tierNumber > 0) {
            identityParts.push("Tier " + nodeModel.tierNumber);
        }
        if (identityParts.length > 0) {
            lines.push(identityParts.join(" | "));
        }

        const costParts = [];
        if (nodeModel.reputationRequirement > 0) {
            costParts.push("Rep " + nodeModel.reputationRequirement);
        }
        if (nodeModel.cashCostText) {
            costParts.push(nodeModel.cashCostText);
        }
        if (nodeModel.statusText) {
            costParts.push(nodeModel.statusText);
        }
        if (costParts.length > 0) {
            lines.push(costParts.join(" | "));
        }

        return lines.join("\n");
    }

    function showTechTooltip(nodeModel, clientX, clientY) {
        if (!techTooltip || !nodeModel) {
            hideTechTooltip();
            return;
        }

        techTooltipTitle.textContent = nodeModel.titleText || "Technology";
        techTooltipMeta.textContent = buildTechTooltipMeta(nodeModel);
        techTooltip.hidden = false;
        moveTechTooltip(clientX, clientY);
    }

    function modifierRemainingHours(modifier) {
        if (!modifier || typeof modifier.remainingGameHours !== "number") {
            return 0;
        }
        return Math.max(0, Math.ceil(modifier.remainingGameHours));
    }

    function formatModifierRemainingHours(totalHours) {
        return Math.max(0, Math.ceil(totalHours)) + "h";
    }

    function buildModifierTooltipMeta(modifier) {
        const description = modifier && modifier.description ? modifier.description : "No details.";
        if (modifier && modifier.timed) {
            return description + " Remaining: " +
                formatModifierRemainingHours(modifierRemainingHours(modifier)) + ".";
        }
        return description;
    }

    function modifierBadgeLabel(modifier) {
        const words = String(modifier && modifier.name ? modifier.name : "Modifier")
            .split(/[^A-Za-z0-9]+/)
            .filter(Boolean);
        if (words.length >= 2) {
            return (words[0][0] + words[1][0]).toUpperCase();
        }
        if (words.length === 1) {
            return words[0].slice(0, 2).toUpperCase();
        }
        return "MD";
    }

    function renderModifierStrip(container, modifiers, timed) {
        if (!container) {
            return;
        }

        container.innerHTML = "";
        container.hidden = modifiers.length === 0;
        modifiers.forEach((modifier) => {
            const badge = document.createElement("button");
            badge.type = "button";
            badge.className = "modifier-badge " + (timed ? "timed" : "permanent");
            badge.setAttribute(
                "aria-label",
                timed
                    ? ((modifier.name || "Modifier") + ", " +
                        formatModifierRemainingHours(modifierRemainingHours(modifier)) + " remaining")
                    : (modifier.name || "Modifier"));

            const label = document.createElement("span");
            label.className = "modifier-badge-label";
            label.textContent = modifierBadgeLabel(modifier);
            badge.appendChild(label);

            const type = document.createElement("span");
            type.className = "modifier-badge-type" + (timed ? " timed-remaining" : "");
            type.textContent = timed
                ? formatModifierRemainingHours(modifierRemainingHours(modifier))
                : "Mod";
            badge.appendChild(type);

            badge.addEventListener("mouseenter", function (event) {
                showModifierTooltip(modifier, event.clientX, event.clientY);
            });
            badge.addEventListener("mousemove", function (event) {
                showModifierTooltip(modifier, event.clientX, event.clientY);
            });
            badge.addEventListener("mouseleave", function () {
                hideModifierTooltip();
            });
            if (timed && modifier.removable && typeof modifier.modifierId === "number") {
                badge.addEventListener("contextmenu", function (event) {
                    event.preventDefault();
                    event.stopPropagation();
                    postEndSiteModifier(modifier.modifierId).catch(() => {
                        statusChip.textContent = "Failed to end modifier.";
                    });
                });
            }

            container.appendChild(badge);
        });
    }

    function renderSiteModifiers(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            if (buffModifierStrip) {
                buffModifierStrip.hidden = true;
                buffModifierStrip.innerHTML = "";
            }
            if (normalModifierStrip) {
                normalModifierStrip.hidden = true;
                normalModifierStrip.innerHTML = "";
            }
            hideModifierTooltip();
            return;
        }

        const modifiers = getActiveModifiers(state);
        const timedModifiers = modifiers.filter((modifier) => !!modifier && modifier.timed);
        const normalModifiers = modifiers.filter((modifier) => !!modifier && !modifier.timed);
        renderModifierStrip(buffModifierStrip, timedModifiers, true);
        renderModifierStrip(normalModifierStrip, normalModifiers, false);
    }

    function clearSelectionInventory() {
        selectionInventory.classList.remove("site-result-actions");
        setTechTreeOverlayActive(false);
        selectionInventory.hidden = true;
        selectionInventory.innerHTML = "";
        hideInventoryTooltip();
        hideModifierTooltip();
    }

    function clampMeterPercent(value) {
        return Math.round(clamp01((typeof value === "number" ? value : 0) / 100) * 100);
    }

    function renderSiteVitalsPanel(state) {
        if (!siteVitalsPanel || !siteVitalsMoney || !siteVitalsReputation || !siteVitalsBars) {
            return;
        }

        if (!state || (state.appState !== "SITE_ACTIVE" && state.appState !== "REGIONAL_MAP")) {
            siteVitalsPanel.hidden = true;
            siteVitalsBars.innerHTML = "";
            renderSiteModifiers(null);
            return;
        }

        const hud = getHudState(state);
        const campaignResources = getCampaignResources(state);
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
        const morale = hud && typeof hud.playerMorale === "number"
            ? hud.playerMorale
            : 0;
        const currentMoney =
            campaignResources && typeof campaignResources.currentMoney === "number"
                ? campaignResources.currentMoney
                : (hud && typeof hud.currentMoney === "number" ? hud.currentMoney : 0);
        const totalReputation =
            campaignResources && typeof campaignResources.totalReputation === "number"
                ? campaignResources.totalReputation
                : 0;

        siteVitalsPanel.hidden = false;
        const cashValue = siteVitalsMoney.querySelector(".resource-value");
        const reputationValue = siteVitalsReputation.querySelector(".resource-value");
        if (cashValue) {
            cashValue.textContent = formatMoney(currentMoney);
        }
        if (reputationValue) {
            reputationValue.textContent = String(totalReputation);
        }
        siteVitalsBars.innerHTML = "";

        if (state.appState !== "SITE_ACTIVE") {
            renderSiteModifiers(null);
            return;
        }

        [
            { label: "Health", percent: clampMeterPercent(health), className: "health" },
            { label: "Hydration", percent: clampMeterPercent(hydration), className: "hydration" },
            { label: "Energy", percent: clampMeterPercent(energy), className: "energy" },
            { label: "Morale", percent: clampMeterPercent(morale), className: "morale" }
        ].forEach((meter) => {
            const bar = document.createElement("div");
            bar.className = "site-vitals-bar " + meter.className;
            bar.style.setProperty("--site-meter-percent", meter.percent + "%");

            const content = document.createElement("div");
            content.className = "site-vitals-bar-content";

            const label = document.createElement("span");
            label.textContent = meter.label;
            content.appendChild(label);

            const value = document.createElement("span");
            value.textContent = meter.percent + "%";
            content.appendChild(value);

            bar.appendChild(content);
            siteVitalsBars.appendChild(bar);
        });

        renderSiteModifiers(state);
    }

    function renderSiteHudChrome(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            renderSiteVitalsPanel(null);
            return;
        }

        const warning = getHudWarningPresentation(state);
        const protectionOverlay = getProtectionOverlayState(state);
        const overlayModeName =
            protectionOverlay && typeof protectionOverlay.mode === "string"
                ? protectionOverlay.mode
                : "NONE";
        hudEyebrow.textContent = warning.code !== hudWarningCodes.none ? warning.headline : "Site Active";
        hudTitle.textContent = getActiveSiteName(state);
        if (overlayModeName !== "NONE") {
            const overlayPresentation = protectionOverlayPresentation(overlayModeName);
            const siteBootstrap = getSiteBootstrap(state);
            const siteTiles = siteBootstrap && Array.isArray(siteBootstrap.tiles)
                ? siteBootstrap.tiles
                : [];
            const peakValue = resolveProtectionOverlayPeakValue(siteTiles, overlayModeName);
            hudSubtitle.textContent =
                overlayPresentation.title +
                " overlay: strongest current final shelter is " +
                peakValue.toFixed(1) +
                " on the 0-100 scale.";
        } else {
            hudSubtitle.textContent = getActiveSiteTip();
        }
        renderSiteVitalsPanel(state);
    }

    function makeInventorySection(title, metaText, gridClassName, columnCount) {
        const section = document.createElement("section");
        section.className = "inventory-section";

        if (title || metaText) {
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
        }

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
            appendUiIcon(icon, visual.iconKey || "idle");
        } else {
            appendUiIcon(icon, "idle");
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

        setTechTreeOverlayActive(false);
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
            "Site 1 starts with its opening loadout already packed in the delivery crate: basic straw checkerboard, one water, and the opening cash reserve.";
        stack.appendChild(footnote);
    }

    function technologyIconPresentation(titleText, kindText) {
        const haystack = (String(kindText || "") + " " + String(titleText || "")).toLowerCase();
        if (/(water|hydr|irrig|pump|well|mist)/.test(haystack)) {
            return { iconKey: "water", light: "#6d91ad", dark: "#3d5c72" };
        }
        if (/(wind|storm|gust|draft|air)/.test(haystack)) {
            return { iconKey: "wind", light: "#8ca48f", dark: "#546a57" };
        }
        if (/(grow|plant|seed|soil|reed|vine|bush|root|flora)/.test(haystack)) {
            return { iconKey: "sprout", light: "#86a266", dark: "#50683f" };
        }
        if (/(craft|forge|build|tool|repair|work|stove|frame)/.test(haystack)) {
            return { iconKey: "hammer", light: "#ac8659", dark: "#6a4d2a" };
        }
        if (/(camp|storage|crate|supply|shelter|logistic|cache)/.test(haystack)) {
            return { iconKey: "box", light: "#9f7a57", dark: "#624628" };
        }
        if (/(crew|worker|hire|team|support|morale|social)/.test(haystack)) {
            return { iconKey: "userPlus", light: "#9f7b95", dark: "#644d60" };
        }
        return { iconKey: "compass", light: "#8d6b3f", dark: "#52391f" };
    }

    function unlockableIconPresentation(unlockableName) {
        const name = String(unlockableName || "").toLowerCase();
        if (name.includes("checkerboard")) {
            return { iconKey: "grid", light: "#c7a66d", dark: "#7c5c34" };
        }
        if (name.includes("wormwood")) {
            return { iconKey: "reed", light: "#7fa569", dark: "#4b6d41" };
        }
        if (name.includes("white thorn")) {
            return { iconKey: "shrub", light: "#8ca766", dark: "#546b3f" };
        }
        if (name.includes("red tamarisk")) {
            return { iconKey: "cactus", light: "#7a9a72", dark: "#49614b" };
        }
        if (name.includes("jiji grass")) {
            return { iconKey: "reed", light: "#9ab36f", dark: "#617744" };
        }
        if (name.includes("sea buckthorn")) {
            return { iconKey: "shrub", light: "#b59a57", dark: "#745b2f" };
        }
        if (name.includes("desert ephedra")) {
            return { iconKey: "sprout", light: "#88a56f", dark: "#536a47" };
        }
        if (name.includes("wolfberry")) {
            return { iconKey: "vine", light: "#99784d", dark: "#61472b" };
        }
        if (name.includes("saxaul")) {
            return { iconKey: "wood", light: "#b28958", dark: "#714d2e" };
        }
        return { iconKey: "sprout", light: "#90a86f", dark: "#596b45" };
    }

    function parseTechTreeUnlockableTiers(techTreeSetup) {
        const tiers = [];
        let parsingUnlockables = false;
        let activeTier = null;

        (techTreeSetup.elements || []).forEach((element) => {
            if (!element) {
                return;
            }

            const text = String(element.text || "").trim();
            if (!text) {
                return;
            }

            if (text === "Total Reputation Unlocks") {
                parsingUnlockables = true;
                activeTier = null;
                return;
            }

            if (!parsingUnlockables) {
                return;
            }

            if (element.action && element.action.type === "SELECT_TECH_TREE_FACTION_TAB") {
                parsingUnlockables = false;
                activeTier = null;
                return;
            }

            if (/^Starter Plants$/i.test(text) ||
                /^Base Tier\b/i.test(text) ||
                /^Faction Tier\b/i.test(text))
            {
                parsingUnlockables = false;
                activeTier = null;
                return;
            }

            if (/^(Base Tech Tree)$/i.test(text) || /^Enhancements\b/i.test(text)) {
                return;
            }

            const tierMatch = text.match(/^Global\s+(\d+)\s+Need\s+(-?\d+)\s+(Ready|Locked)$/i);
            if (tierMatch) {
                activeTier = {
                    tierNumber: Number(tierMatch[1]),
                    requirement: Number(tierMatch[2]),
                    isUnlocked: /^ready$/i.test(tierMatch[3]),
                    unlockables: []
                };
                tiers.push(activeTier);
                return;
            }

            const hasNoAction = !element.action || element.action.type === "NONE";
            if (!hasNoAction || !activeTier) {
                return;
            }

            const unlockableMatch = text.match(/^(.*?)\s+(Ready|Locked)$/i);
            if (!unlockableMatch) {
                return;
            }

            activeTier.unlockables.push({
                name: unlockableMatch[1].trim(),
                isUnlocked: /^ready$/i.test(unlockableMatch[2])
            });
        });

        return tiers;
    }

    function renderUnlockablesTabContent(container, unlockableTiers) {
        const tierStack = document.createElement("div");
        tierStack.className = "tech-tree-tier-stack";
        container.appendChild(tierStack);

        if (unlockableTiers.length === 0) {
            const emptyState = document.createElement("div");
            emptyState.className = "tech-tree-empty-state";

            const emptyTitle = document.createElement("div");
            emptyTitle.className = "tech-tree-empty-title";
            emptyTitle.textContent = "No unlockables projected";
            emptyState.appendChild(emptyTitle);

            const emptyCopy = document.createElement("div");
            emptyCopy.className = "tech-tree-empty-copy";
            emptyCopy.textContent = "Reputation tiers will appear here once the runtime sends them.";
            emptyState.appendChild(emptyCopy);

            tierStack.appendChild(emptyState);
            return;
        }

        unlockableTiers.forEach((tier) => {
            const tierCard = document.createElement("section");
            tierCard.className = "unlock-tier-card";
            tierCard.classList.add(tier.isUnlocked ? "unlocked" : "locked");
            tierStack.appendChild(tierCard);

            const tierContent = document.createElement("div");
            tierContent.className = "unlock-tier-content";
            tierCard.appendChild(tierContent);

            const header = document.createElement("div");
            header.className = "unlock-tier-header";
            tierContent.appendChild(header);

            const badge = document.createElement("div");
            badge.className = "unlock-tier-badge";
            header.appendChild(badge);

            const badgeNumber = document.createElement("div");
            badgeNumber.className = "unlock-tier-number";
            badgeNumber.textContent = String(tier.tierNumber);
            badge.appendChild(badgeNumber);

            if (!tier.isUnlocked) {
                const badgeLock = document.createElement("div");
                badgeLock.className = "unlock-tier-badge-lock";
                appendUiIcon(badgeLock, "lock");
                badge.appendChild(badgeLock);
            }

            const headerCopy = document.createElement("div");
            headerCopy.className = "unlock-tier-copy";
            header.appendChild(headerCopy);

            const title = document.createElement("div");
            title.className = "unlock-tier-title";
            title.textContent = "Reputation Tier " + tier.tierNumber;
            headerCopy.appendChild(title);

            const subtitle = document.createElement("div");
            subtitle.className = "unlock-tier-meta";
            subtitle.textContent = "Need Total Reputation " + tier.requirement;
            headerCopy.appendChild(subtitle);

            const unlockableGrid = document.createElement("div");
            unlockableGrid.className = "unlockable-grid";
            tierContent.appendChild(unlockableGrid);

            tier.unlockables.forEach((unlockable) => {
                const entry = document.createElement("article");
                entry.className = "unlockable-entry";
                unlockableGrid.appendChild(entry);

                const iconFrame = document.createElement("div");
                iconFrame.className = "unlockable-icon";
                const iconPresentation = unlockableIconPresentation(unlockable.name);
                iconFrame.style.setProperty("--unlock-icon-light", iconPresentation.light);
                iconFrame.style.setProperty("--unlock-icon-dark", iconPresentation.dark);
                appendUiIcon(iconFrame, iconPresentation.iconKey);
                entry.appendChild(iconFrame);

                if (!tier.isUnlocked) {
                    const iconLock = document.createElement("div");
                    iconLock.className = "unlockable-lock";
                    appendUiIcon(iconLock, "lock");
                    iconFrame.appendChild(iconLock);
                }

                const entryLabel = document.createElement("div");
                entryLabel.className = "unlockable-name";
                entryLabel.textContent = unlockable.name;
                entry.appendChild(entryLabel);
            });

            if (!tier.isUnlocked) {
                const overlay = document.createElement("div");
                overlay.className = "unlock-tier-overlay";
                tierCard.appendChild(overlay);
            }
        });
    }

    function renderTechTreeEmptyState(container) {
        const emptyState = document.createElement("div");
        emptyState.className = "tech-tree-empty-state";
        container.appendChild(emptyState);

        const emptyKicker = document.createElement("div");
        emptyKicker.className = "tech-tree-empty-kicker";
        emptyKicker.textContent = "Design Pending";
        emptyState.appendChild(emptyKicker);

        const emptyTitle = document.createElement("div");
        emptyTitle.className = "tech-tree-empty-title";
        emptyTitle.textContent = "Tech tree layout coming later";
        emptyState.appendChild(emptyTitle);

        const emptyCopy = document.createElement("div");
        emptyCopy.className = "tech-tree-empty-copy";
        emptyCopy.textContent =
            "This tab is intentionally empty for now while the structure and interaction design are still being defined.";
        emptyState.appendChild(emptyCopy);
    }

    function isTechnologyNodeActionType(actionType) {
        return actionType === "CLAIM_TECHNOLOGY_NODE" || actionType === "REFUND_TECHNOLOGY_NODE";
    }

    function normalizeTechFactionName(name) {
        const normalized = String(name || "").trim().toLowerCase();
        if (normalized.includes("village")) {
            return "village committee";
        }
        if (normalized.includes("forestry") || normalized.includes("bureau")) {
            return "forestry bureau";
        }
        if (normalized.includes("university")) {
            return "agricultural university";
        }
        return normalized;
    }

    function displayTechFactionLabel(name) {
        const normalized = normalizeTechFactionName(name);
        if (normalized === "village committee") {
            return "Village";
        }
        if (normalized === "forestry bureau") {
            return "Bureau";
        }
        if (normalized === "agricultural university") {
            return "University";
        }
        return String(name || "Faction");
    }

    function expandCompactTechStatus(statusText) {
        const compactStatus = String(statusText || "").trim();
        if (!compactStatus.length) {
            return "";
        }

        let match = compactStatus.match(/^Need\s+(\d+)r\s+\+(\$\d+(?:\.\d{2})?)$/i);
        if (match) {
            return "Need " + match[1] + " rep + " + match[2];
        }

        match = compactStatus.match(/^Need\s+(\d+)r$/i);
        if (match) {
            return "Need " + match[1] + " rep";
        }

        return compactStatus;
    }

    function technologyNodeIdFromParts(factionId, tierIndex) {
        return 1000 + factionId * 1000 + tierIndex * 10;
    }

    function parseSimpleTomlValue(rawValue) {
        const value = String(rawValue || "").trim();
        if (!value.length) {
            return "";
        }
        if (value[0] === '"' && value[value.length - 1] === '"') {
            return value.slice(1, value.length - 1).replace(/\\"/g, '"');
        }
        const parsedNumber = Number(value);
        if (Number.isFinite(parsedNumber)) {
            return parsedNumber;
        }
        if (/^(true|false)$/i.test(value)) {
            return /^true$/i.test(value);
        }
        return value;
    }

    function parseTomlTableArray(tomlText, tableName) {
        const marker = "[[" + tableName + "]]";
        const lines = String(tomlText || "").split(/\r?\n/);
        const rows = [];
        let currentRow = null;
        lines.forEach((line) => {
            const trimmed = line.trim();
            if (!trimmed || trimmed[0] === "#") {
                return;
            }
            if (trimmed === marker) {
                if (currentRow) {
                    rows.push(currentRow);
                }
                currentRow = {};
                return;
            }
            if (!currentRow) {
                return;
            }
            const equalsIndex = trimmed.indexOf("=");
            if (equalsIndex <= 0) {
                return;
            }
            const key = trimmed.slice(0, equalsIndex).trim();
            const value = trimmed.slice(equalsIndex + 1).trim();
            currentRow[key] = parseSimpleTomlValue(value);
        });
        if (currentRow) {
            rows.push(currentRow);
        }
        return rows;
    }

    function buildTechnologyCatalog(nodeTomlText, tierTomlText) {
        const catalog = {
            nodesById: new Map(),
            tiersByIndex: new Map()
        };
        parseTomlTableArray(tierTomlText, "technology_tiers").forEach((row) => {
            const tierIndex = Number(row.tier_index || 0);
            if (tierIndex > 0) {
                catalog.tiersByIndex.set(tierIndex, {
                    tierIndex: tierIndex,
                    displayName: String(row.display_name || ("Tier " + tierIndex))
                });
            }
        });
        parseTomlTableArray(nodeTomlText, "technology_nodes").forEach((row) => {
            const factionId = Number(row.faction_id || 0);
            const tierIndex = Number(row.tier_index || 0);
            const nodeId = technologyNodeIdFromParts(factionId, tierIndex);
            catalog.nodesById.set(nodeId, {
                nodeId: nodeId,
                factionId: factionId,
                tierIndex: tierIndex,
                displayName: String(row.display_name || ""),
                description: String(row.description || ""),
                internalCostCashPoints: Number(row.internal_cost_cash_points || 0),
                reputationRequirement: Number(row.reputation_requirement || 0),
                grantedContentKind: String(row.granted_content_kind || ""),
                grantedContentId: Number(row.granted_content_id || 0)
            });
        });
        return catalog;
    }

    function ensureTechnologyCatalog() {
        if (technologyCatalog) {
            return Promise.resolve(technologyCatalog);
        }
        if (technologyCatalogPromise) {
            return technologyCatalogPromise;
        }
        technologyCatalogPromise = Promise.all([
            fetch("/content/technology_nodes.toml", { cache: "no-store" }).then((response) => response.text()),
            fetch("/content/technology_tiers.toml", { cache: "no-store" }).then((response) => response.text())
        ]).then((results) => {
            technologyCatalog = buildTechnologyCatalog(results[0], results[1]);
            technologyCatalogPromise = null;
            if (latestState) {
                renderState(latestState, {});
            }
            return technologyCatalog;
        }).catch((error) => {
            technologyCatalogPromise = null;
            reportViewerError("ensureTechnologyCatalog", error);
            throw error;
        });
        return technologyCatalogPromise;
    }

    function isPotentialTechnologyNodeElement(element) {
        if (!element || element.elementType !== "BUTTON" || (element.flags & 4) !== 0) {
            return false;
        }

        const text = String(element.text || "").trim();
        if (!text) {
            return false;
        }

        if (/^Tab:\s+/i.test(text)) {
            return false;
        }

        return /^TECHNODE\|/i.test(text) || /\bT\d+\b/i.test(text) || /\bRefund\b/i.test(text);
    }

    function parseTechTreeActionElement(element) {
        if (!element) {
            return null;
        }

        const text = String(element.text || "").trim();
        if (/^TECHNODE\|/i.test(text)) {
            const fields = {};
            text.split("|").slice(1).forEach((part) => {
                const separatorIndex = part.indexOf("=");
                if (separatorIndex <= 0) {
                    return;
                }
                const key = part.slice(0, separatorIndex).trim().toLowerCase();
                const value = part.slice(separatorIndex + 1).trim();
                fields[key] = value;
            });
            const parsedTierNumber = Number(fields.tier || 0);
            const targetId = element.action && typeof element.action.targetId === "number" ? element.action.targetId : 0;
            const catalogNode =
                technologyCatalog && targetId ? technologyCatalog.nodesById.get(targetId) : null;
            const compactStatusText = fields.status || fields.s || "";
            const statusTextFromAction = (() => {
                if (element.action && element.action.type === "REFUND_TECHNOLOGY_NODE") {
                    return "Refund";
                }
                if (element.action && element.action.type === "CLAIM_TECHNOLOGY_NODE") {
                    const repRequirement = catalogNode ? catalogNode.reputationRequirement : 0;
                    const cashCost =
                        catalogNode ? formatMoney((catalogNode.internalCostCashPoints || 0) / 100) : "0.00";
                    return "Need " + repRequirement + " rep + $" + cashCost;
                }
                return (element.flags & 2) !== 0 ? "Locked" : "Unavailable";
            })();
            const resolvedStatusText = expandCompactTechStatus(compactStatusText) || statusTextFromAction;
            return {
                element: element,
                titleText: (catalogNode && catalogNode.displayName) || fields.title || text,
                kindText: fields.kind || "Tech",
                factionText:
                    (catalogNode
                        ? ({
                            1: "Village Committee",
                            2: "Forestry Bureau",
                            3: "Agricultural University"
                        }[catalogNode.factionId] || "")
                        : (fields.faction || "")),
                tierNumber:
                    (catalogNode && catalogNode.tierIndex) ||
                    (Number.isFinite(parsedTierNumber) ? parsedTierNumber : 0),
                descriptionText: (catalogNode && catalogNode.description) || fields.desc || "",
                statusText: resolvedStatusText,
                reputationRequirement: catalogNode ? Number(catalogNode.reputationRequirement || 0) : 0,
                cashCostText:
                    catalogNode
                        ? ("$" + formatMoney((catalogNode.internalCostCashPoints || 0) / 100))
                        : "",
                enhancementChoiceIndex: 0,
                isEnhancement: false,
                costText: (() => {
                    const effectiveStatus = resolvedStatusText;
                    const matchedCash = effectiveStatus.match(/\$\d+(?:\.\d{2})?/);
                    const matchedRep = effectiveStatus.match(/Need\s+(\d+)\s+rep/i);
                    if (matchedCash) {
                        return matchedCash[0];
                    }
                    if (matchedRep && matchedRep[1]) {
                        return "Rep " + matchedRep[1];
                    }
                    return catalogNode ? ("T" + String(catalogNode.tierIndex || "")) : ("T" + String(fields.tier || ""));
                })(),
                isClaimable: (element.flags & 1) !== 0,
                isDisabled: (element.flags & 2) !== 0,
                isClaimed: /Claimed/i.test(resolvedStatusText),
                isLocked: /Locked|Unavailable|Need/i.test(resolvedStatusText)
            };
        }

        const parts = text
            .split("|")
            .map((part) => part.trim())
            .filter(Boolean);
        const leftText = parts[0] || "";
        const detailParts = parts.length > 1 ? parts.slice(1) : [];
        const leftMatch = leftText.match(/^(.*?)\s+T(\d+)(?:\s+(.*))?$/i);
        const factionText = leftMatch ? leftMatch[1].trim() : "";
        const kindText = "Tech";
        const tierNumber = leftMatch ? Number(leftMatch[2]) : 0;
        const primaryStatusText = leftMatch && leftMatch[3] ? leftMatch[3].trim() : leftText;
        let titleText = "";
        let descriptionText = "";
        const filteredDetailParts = [];
        detailParts.forEach((part) => {
            if (/^About\s+/i.test(part)) {
                descriptionText = part.replace(/^About\s+/i, "").trim();
            } else if (!titleText &&
                !/^(Param|Unlock|Need|Claimed|Refund|\+\$|Unavailable|Other enhancement|Need paired)/i.test(part))
            {
                titleText = part.trim();
            } else {
                filteredDetailParts.push(part);
            }
        });
        if (!titleText) {
            titleText = leftText;
        }
        const statusText = [primaryStatusText].concat(filteredDetailParts).filter(Boolean).join(" | ");
        const costCashMatch = statusText.match(/\$\d+(?:\.\d{2})?/);
        const costRepMatch = statusText.match(/Need\s+(\d+)\s+rep/i);
        const costText = costCashMatch
            ? costCashMatch[0]
            : (costRepMatch ? ("Rep " + costRepMatch[1]) : ("T" + String(tierNumber || "")));

        return {
            element: element,
            titleText: titleText,
            kindText: kindText,
            factionText: factionText,
            tierNumber: tierNumber,
            descriptionText: descriptionText,
            statusText: statusText,
            reputationRequirement: 0,
            cashCostText: "",
            enhancementChoiceIndex: 0,
            isEnhancement: false,
            costText: costText,
            isClaimable: (element.flags & 1) !== 0,
            isDisabled: (element.flags & 2) !== 0,
            isClaimed: /Claimed/i.test(statusText),
            isLocked: /Locked/i.test(statusText)
        };
    }

    function buildTechTreeNodeButton(nodeModel, extraClassName) {
        const nodeButton = document.createElement("button");
        nodeButton.type = "button";
        nodeButton.className = extraClassName;
        nodeButton.setAttribute("aria-disabled", nodeModel.isDisabled ? "true" : "false");
        if (nodeModel.isClaimable) {
            nodeButton.classList.add("claimable");
        }
        if (nodeModel.isDisabled) {
            nodeButton.classList.add("disabled");
        }
        if (nodeModel.isClaimed) {
            nodeButton.classList.add("claimed");
        } else if (nodeModel.isLocked) {
            nodeButton.classList.add("locked");
        }
        if (nodeModel.element.action && nodeModel.element.action.type !== "NONE" && !nodeModel.isDisabled) {
            bindReliablePrimaryPress(nodeButton, function () {
                postJson("/ui-action", nodeModel.element.action).catch(() => {
                    statusChip.textContent = "Failed to send tech-tree action.";
                });
            });
        }
        nodeButton.addEventListener("mouseenter", function (event) {
            showTechTooltip(nodeModel, event.clientX, event.clientY);
        });
        nodeButton.addEventListener("mousemove", function (event) {
            moveTechTooltip(event.clientX, event.clientY);
        });
        nodeButton.addEventListener("mouseleave", function () {
            hideTechTooltip();
        });

        const nodeTop = document.createElement("div");
        nodeTop.className = "tech-node-top";
        nodeButton.appendChild(nodeTop);

        const nodeIcon = document.createElement("div");
        nodeIcon.className = "tech-node-icon";
        const iconPresentation = technologyIconPresentation(nodeModel.titleText, nodeModel.kindText);
        nodeIcon.style.setProperty("--tech-icon-light", iconPresentation.light);
        nodeIcon.style.setProperty("--tech-icon-dark", iconPresentation.dark);
        appendUiIcon(nodeIcon, iconPresentation.iconKey);
        nodeTop.appendChild(nodeIcon);
        if (!nodeModel.isClaimed) {
            nodeIcon.classList.add("locked-visual");
            const nodeLock = document.createElement("div");
            nodeLock.className = "tech-node-lock";
            appendUiIcon(nodeLock, "lock");
            nodeIcon.appendChild(nodeLock);
        }

        const nodeTitle = document.createElement("div");
        nodeTitle.className = "tech-node-title";
        nodeTitle.textContent = nodeModel.titleText;
        nodeButton.appendChild(nodeTitle);

        return nodeButton;
    }

    function techRowRequirementText(nodes) {
        const requirements = Array.from(new Set((nodes || [])
            .map((nodeModel) => Number(nodeModel.reputationRequirement || 0))
            .filter((value) => value > 0)))
            .sort(function (left, right) { return left - right; });
        if (requirements.length === 0) {
            return "Rep --";
        }
        if (requirements.length === 1) {
            return "Rep " + requirements[0];
        }
        return "Rep " + requirements[0] + "-" + requirements[requirements.length - 1];
    }

    function buildTechRequirementCell(labelText, nodes, extraClassName) {
        const requirementCell = document.createElement("div");
        requirementCell.className = "tech-row-requirement " + extraClassName;

        const requirementLabel = document.createElement("div");
        requirementLabel.className = "tech-row-label";
        requirementLabel.textContent = labelText;
        requirementCell.appendChild(requirementLabel);

        const requirementValue = document.createElement("div");
        requirementValue.className = "tech-row-value";
        requirementValue.textContent = techRowRequirementText(nodes);
        requirementCell.appendChild(requirementValue);

        return requirementCell;
    }

    function parseTechTreeStructure(techTreeSetup) {
        const parsed = {
            factionTabs: [],
            tiers: [],
            selectedFactionName: ""
        };
        if (!techTreeSetup || !Array.isArray(techTreeSetup.elements)) {
            return parsed;
        }

        let activeTier = null;
        techTreeSetup.elements.forEach((element) => {
            if (!element) {
                return;
            }

            const text = String(element.text || "").trim();
            const actionType = element.action ? element.action.type : "NONE";
            if (!text || (element.flags & 4) !== 0) {
                return;
            }

            if (actionType === "SELECT_TECH_TREE_FACTION_TAB") {
                const label = text.replace(/^Tab:\s*/i, "").trim();
                const tabModel = {
                    element: element,
                    label: label,
                    isActive: (element.flags & 1) !== 0
                };
                parsed.factionTabs.push(tabModel);
                if (tabModel.isActive && label) {
                    parsed.selectedFactionName = label;
                }
                activeTier = null;
                return;
            }

            const tierMatch = text.match(/^Tier\s+(\d+)\s+\|\s+(.*)$/i);
            if (element.elementType === "LABEL" || !element.action) {
                if (tierMatch) {
                    activeTier = {
                        tierNumber: Number(tierMatch[1]),
                        titleText: tierMatch[2].trim(),
                        nodes: []
                    };
                    parsed.tiers.push(activeTier);
                }
                return;
            }

            if (!activeTier || !isPotentialTechnologyNodeElement(element)) {
                return;
            }

            const nodeModel = parseTechTreeActionElement(element);
            if (nodeModel) {
                activeTier.nodes.push(nodeModel);
            }
        });

        if (!parsed.selectedFactionName && parsed.factionTabs.length > 0) {
            parsed.selectedFactionName = parsed.factionTabs[0].label;
        }
        return parsed;
    }

    function makeTechPlaceholder(labelText, className) {
        const placeholder = document.createElement("div");
        placeholder.className = className + " tech-placeholder";
        placeholder.textContent = labelText;
        return placeholder;
    }

    function renderTechTreeTabContent(container, parsedTree) {
        const tierStack = document.createElement("div");
        tierStack.className = "tech-tree-tier-stack";
        container.appendChild(tierStack);

        if (!technologyCatalog) {
            ensureTechnologyCatalog().catch(() => {});
            const emptyState = document.createElement("div");
            emptyState.className = "tech-tree-empty-state";
            container.appendChild(emptyState);
            const emptyTitle = document.createElement("div");
            emptyTitle.className = "tech-tree-empty-title";
            emptyTitle.textContent = "Loading technology catalog";
            emptyState.appendChild(emptyTitle);
            const emptyCopy = document.createElement("div");
            emptyCopy.className = "tech-tree-empty-copy";
            emptyCopy.textContent = "Waiting for authored technology data from the local smoke host.";
            emptyState.appendChild(emptyCopy);
            return;
        }

        if (!parsedTree || !Array.isArray(parsedTree.tiers) || parsedTree.tiers.length === 0) {
            renderTechTreeEmptyState(container);
            return;
        }

        const factionTabs = parsedTree && Array.isArray(parsedTree.factionTabs) ? parsedTree.factionTabs : [];

        parsedTree.tiers.forEach((tierModel) => {
            const factionNodes = tierModel.nodes.slice();
            if (factionNodes.length === 0) {
                return;
            }

            const tierCard = document.createElement("section");
            tierCard.className = "tech-tier-card";
            tierStack.appendChild(tierCard);

            const tierTitle = document.createElement("div");
            tierTitle.className = "tech-tier-title";
            const catalogTier = technologyCatalog.tiersByIndex.get(tierModel.tierNumber);
            tierTitle.textContent =
                "Tier " + tierModel.tierNumber + " | " +
                (catalogTier && catalogTier.displayName ? catalogTier.displayName : tierModel.titleText);
            tierCard.appendChild(tierTitle);

            const tierMeta = document.createElement("div");
            tierMeta.className = "tech-tier-meta";
            tierMeta.textContent = "Each faction now advances through one linear tech per tier, with faction reputation matching the tier number.";
            tierCard.appendChild(tierMeta);

            const tierGrid = document.createElement("div");
            tierGrid.className = "tech-tier-grid";
            tierCard.appendChild(tierGrid);

            const factionOrder =
                factionTabs.length > 0
                    ? factionTabs.map((tabModel) => tabModel.label)
                    : Array.from(new Set(factionNodes.map((nodeModel) => nodeModel.factionText)));

            const gridSpacer = document.createElement("div");
            gridSpacer.className = "tech-grid-spacer";
            tierGrid.appendChild(gridSpacer);

            factionOrder.forEach((factionName) => {
                const heading = document.createElement("div");
                heading.className = "tech-faction-heading";
                heading.textContent = displayTechFactionLabel(factionName);
                tierGrid.appendChild(heading);
            });

            tierGrid.appendChild(buildTechRequirementCell("Faction Tech", factionNodes, "base"));
            factionOrder.forEach((factionName) => {
                const normalizedFactionName = normalizeTechFactionName(factionName);
                const columnNodes = factionNodes.filter((nodeModel) => {
                    return normalizeTechFactionName(nodeModel.factionText) === normalizedFactionName;
                });

                const column = document.createElement("div");
                column.className = "tech-row-cell";
                const linearNode = columnNodes[0] || null;
                if (linearNode) {
                    column.appendChild(buildTechTreeNodeButton(linearNode, "tech-base-card"));
                } else {
                    column.appendChild(makeTechPlaceholder("No Tech", "tech-base-card"));
                }
                tierGrid.appendChild(column);
            });
        });

        if (!tierStack.children.length) {
            renderTechTreeEmptyState(container);
        }
    }

    function renderTechTreePanel(techTreeSetup, options) {
        if (!techTreeSetup) {
            return false;
        }

        const panelOptions = options || {};
        const closeAction = getTechTreeCloseAction(techTreeSetup);
        const labelElements = getLabelElements(techTreeSetup);
        const title = labelElements[0];
        const subtitle = labelElements[1];
        const factionSummary = labelElements[2];
        const titleText = title ? title.text : "Prototype Tech Tree";
        const subtitleText = subtitle ? subtitle.text : "Claim nodes with available faction reputation.";
        const summaryParts = splitInfoParts(subtitleText);
        if (factionSummary && factionSummary.text) {
            summaryParts.push(factionSummary.text);
        }
        const unlockableTiers = parseTechTreeUnlockableTiers(techTreeSetup);
        const parsedTree = parseTechTreeStructure(techTreeSetup);
        if (activeTechTreePanelTabId !== "unlockables" && activeTechTreePanelTabId !== "tech-tree") {
            activeTechTreePanelTabId = "unlockables";
        }

        hideTechTooltip();
        setTechTreeOverlayActive(true);
        selectionInventory.hidden = false;
        selectionInventory.innerHTML = "";

        const section = document.createElement("section");
        section.className = "inventory-section tech-tree-panel";
        selectionInventory.appendChild(section);

        const header = document.createElement("div");
        header.className = "tech-tree-header";
        section.appendChild(header);

        const headerCopy = document.createElement("div");
        headerCopy.className = "tech-tree-header-copy";
        header.appendChild(headerCopy);

        const kickerElement = document.createElement("div");
        kickerElement.className = "tech-tree-kicker";
        kickerElement.textContent = panelOptions.kickerText || "Campaign Research";
        headerCopy.appendChild(kickerElement);

        const titleElement = document.createElement("div");
        titleElement.className = "tech-tree-heading";
        titleElement.textContent = titleText;
        headerCopy.appendChild(titleElement);

        const closeButton = document.createElement("button");
        closeButton.type = "button";
        closeButton.className = "tech-tree-close";
        closeButton.textContent = "Close";
        bindReliablePrimaryPress(closeButton, function () {
            postJson("/ui-action", closeAction).catch(() => {
                statusChip.textContent = "Failed to close tech-tree panel.";
            });
        });
        header.appendChild(closeButton);

        if (summaryParts.length > 0) {
            const summaryRow = document.createElement("div");
            summaryRow.className = "tech-tree-summary-row";
            section.appendChild(summaryRow);

            summaryParts.forEach((part) => {
                const chip = document.createElement("div");
                chip.className = "tech-tree-summary-chip";
                if (/Faction/i.test(part)) {
                    chip.classList.add("active-faction");
                }
                chip.textContent = part;
                summaryRow.appendChild(chip);
            });
        }

        const tabRow = document.createElement("div");
        tabRow.className = "tech-tree-tabs";
        section.appendChild(tabRow);

        [
            { id: "unlockables", label: "Unlockables" },
            { id: "tech-tree", label: "Tech Tree" }
        ].forEach((tab) => {
            const button = document.createElement("button");
            button.type = "button";
            button.className = "tech-tree-tab";
            if (tab.id === activeTechTreePanelTabId) {
                button.classList.add("active");
            }
            button.textContent = tab.label;
            bindReliablePrimaryPress(button, function () {
                activeTechTreePanelTabId = tab.id;
                renderTechTreePanel(techTreeSetup, panelOptions);
            });
            tabRow.appendChild(button);
        });

        const panelBody = document.createElement("div");
        panelBody.className = "tech-tree-body";
        section.appendChild(panelBody);

        if (activeTechTreePanelTabId === "unlockables") {
            renderUnlockablesTabContent(panelBody, unlockableTiers);
        } else {
            renderTechTreeTabContent(panelBody, parsedTree);
        }

        return true;
    }

    function appendSiteTechTreeToggleAction(techTreeSetup) {
        const isOpen = !!techTreeSetup;
        const action = isOpen
            ? getTechTreeCloseAction(techTreeSetup)
            : makeUiAction("OPEN_REGIONAL_MAP_TECH_TREE", 0, 0, 0);

        contextActions.appendChild(
            makeButton(
                isOpen ? "Close Tech Tree" : "Open Tech Tree",
                function () {
                    postJson("/ui-action", action).catch(() => {
                        statusChip.textContent = "Failed to toggle tech-tree panel.";
                    });
                },
                isOpen,
                false
            )
        );
    }

    function renderSiteInventoryPanel(state, workerPackSlots) {
        setTechTreeOverlayActive(false);
        inventoryPanelOpen = isWorkerPackPanelOpen(state);
        if (!inventoryPanelOpen) {
            selectionChip.hidden = true;
            selectionInventory.hidden = true;
            selectionInventory.innerHTML = "";
            hideInventoryTooltip();
            return;
        }

        const workerPackContainer = findWorkerPackInventoryContainer(state);
        const slotCount = Math.max(
            workerPackContainer && typeof workerPackContainer.slotCount === "number"
                ? workerPackContainer.slotCount
                : 0,
            workerPackSlots.length,
            1);

        selectionChip.hidden = false;
        selectionInventory.hidden = false;
        selectionInventory.innerHTML = "";

        const stack = document.createElement("div");
        stack.className = "site-panel-stack";
        selectionInventory.appendChild(stack);

        appendInventoryGridSection(stack, "", "", workerPackSlots, {
            state: state,
            containerKind: "WORKER_PACK",
            slotCount: slotCount,
            columns: 4,
            slotLabelPrefix: "Pack",
            selectedSlotKey: selectedInventorySlotKey
        });
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
            ? ((currentOpenedContainer.flags & inventoryStorageFlags.deliveryBox) !== 0
                ? ("Delivery crate storage - " + slotCount + " slots")
                : (structureMeta
                    ? ("Device storage - " + slotCount + " slots")
                    : "Device storage"))
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
        phoneBodyScrollTop = 0;
        Object.keys(phoneSectionScrollTops).forEach(function (key) {
            delete phoneSectionScrollTops[key];
        });
        phonePointerInteractionActive = false;
        phoneWheelInteractionUntil = 0;
        phoneActiveScrollRegionKey = "";
        if (phoneInteractionResumeTimer !== 0) {
            window.clearTimeout(phoneInteractionResumeTimer);
            phoneInteractionResumeTimer = 0;
        }
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

    function makePhoneSection(title, metaText) {
        const section = document.createElement("section");
        section.className = "phone-section";

        const header = document.createElement("div");
        header.className = "phone-section-header";

        const titleElement = document.createElement("div");
        titleElement.className = "phone-section-title";
        titleElement.textContent = title;
        header.appendChild(titleElement);

        if (metaText) {
            const metaElement = document.createElement("div");
            metaElement.className = "phone-section-meta";
            metaElement.textContent = metaText;
            header.appendChild(metaElement);
        }

        section.appendChild(header);
        return section;
    }

    function appendPhoneSectionContent(section, scrollable) {
        const content = document.createElement("div");
        content.className = "phone-section-content" + (scrollable ? " scrollable" : "");
        section.appendChild(content);
        return content;
    }

    function schedulePhonePanelRefreshAfterInteraction() {
        if (phoneInteractionResumeTimer !== 0) {
            window.clearTimeout(phoneInteractionResumeTimer);
        }

        phoneInteractionResumeTimer = window.setTimeout(function () {
            phoneInteractionResumeTimer = 0;
            if (phonePanelOpen && latestState && phoneActiveScrollRegionKey === "") {
                renderPhonePanel(latestState);
            }
        }, 260);
    }

    function isPhoneInteractionLocked() {
        return phonePointerInteractionActive ||
            performance.now() < phoneWheelInteractionUntil ||
            phoneActiveScrollRegionKey !== "";
    }

    function notePhoneScrollInteraction() {
        phoneWheelInteractionUntil = performance.now() + 260;
        capturePhoneScrollPositions();
        schedulePhonePanelRefreshAfterInteraction();
    }

    function bindPhoneScrollableRegion(content, scrollKey) {
        if (!scrollKey) {
            return;
        }

        content.setAttribute("data-phone-scroll-key", scrollKey);

        content.addEventListener("mouseenter", function () {
            if (content.scrollHeight <= content.clientHeight + 1) {
                return;
            }

            phoneActiveScrollRegionKey = scrollKey;
            capturePhoneScrollPositions();
        });

        content.addEventListener("mouseleave", function () {
            if (phoneActiveScrollRegionKey !== scrollKey) {
                return;
            }

            phoneActiveScrollRegionKey = "";
            capturePhoneScrollPositions();
            schedulePhonePanelRefreshAfterInteraction();
        });
    }

    function capturePhoneScrollPositions() {
        phoneBodyScrollTop = phoneScreenBody.scrollTop;
        phoneScreenBody.querySelectorAll("[data-phone-scroll-key]").forEach(function (element) {
            const key = element.getAttribute("data-phone-scroll-key");
            if (!key) {
                return;
            }
            phoneSectionScrollTops[key] = element.scrollTop;
        });
    }

    function restorePhoneScrollPositions() {
        phoneScreenBody.scrollTop = phoneBodyScrollTop;
        phoneScreenBody.querySelectorAll("[data-phone-scroll-key]").forEach(function (element) {
            const key = element.getAttribute("data-phone-scroll-key");
            if (!key) {
                return;
            }
            element.scrollTop = phoneSectionScrollTops[key] || 0;
        });
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
        const parts = ["$" + formatMoney(listing.price)];
        if (listing.listingKind === "SELL_ITEM") {
            parts.push("available x" + listing.quantity);
        } else if (listing.listingKind === "HIRE_CONTRACTOR") {
            parts.push("open slots x" + listing.quantity);
        } else if (listing.listingKind === "BUY_ITEM") {
            parts.push("stock x" + listing.quantity);
            parts.push("in cart x" + (listing.cartQuantity || 0));
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
            return "Sell For $" + formatMoney(listing.price);
        case "HIRE_CONTRACTOR":
            return "Hire For $" + formatMoney(listing.price);
        case "PURCHASE_UNLOCKABLE":
            return "Unlock For $" + formatMoney(listing.price);
        case "BUY_ITEM":
        default:
            return "Buy For $" + formatMoney(listing.price);
        }
    }

    function getPhoneCartListings(state) {
        return getBuyListings(state).filter((listing) => (listing.cartQuantity || 0) > 0);
    }

    function getPhoneCartItemCount(state) {
        return getPhoneCartListings(state).reduce((total, listing) => total + (listing.cartQuantity || 0), 0);
    }

    function getPhoneCartSubtotal(state) {
        return getPhoneCartListings(state).reduce(
            (total, listing) => total + ((listing.price || 0) * (listing.cartQuantity || 0)),
            0
        );
    }

    function canAddPhoneListingToCart(listing) {
        const cartQuantity = listing.cartQuantity || 0;
        return (listing.quantity || 0) === 0 || cartQuantity < listing.quantity;
    }

    function canCheckoutPhoneCart(state) {
        const hud = getHudState(state);
        const currentMoney = hud && typeof hud.currentMoney === "number" ? hud.currentMoney : 0;
        const subtotal = getPhoneCartSubtotal(state);
        const cartItemCount = getPhoneCartItemCount(state);
        return cartItemCount > 0 && currentMoney >= (subtotal + phoneDeliveryFee);
    }

    function canUsePhoneListing(state, listing) {
        if (listing.listingKind === "SELL_ITEM") {
            return listing.quantity > 0;
        }
        if (listing.listingKind === "BUY_ITEM") {
            return canAddPhoneListingToCart(listing);
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

    function postPhoneCartAction(type, listing) {
        return postJson("/ui-action", {
            type: type,
            targetId: listing.listingId,
            arg0: 1,
            arg1: 0
        });
    }

    function postPhoneCartCheckout() {
        return postJson("/ui-action", {
            type: "CHECKOUT_PHONE_CART",
            targetId: 0,
            arg0: 0,
            arg1: 0
        });
    }

    function getPhonePanelSectionArg(sectionName) {
        switch (sectionName) {
        case "TASKS":
            return 1;
        case "BUY":
            return 2;
        case "SELL":
            return 3;
        case "HIRE":
            return 4;
        case "CART":
            return 5;
        case "HOME":
        default:
            return 0;
        }
    }

    function postPhonePanelSection(sectionName) {
        return postJson("/ui-action", {
            type: "SET_PHONE_PANEL_SECTION",
            targetId: 0,
            arg0: getPhonePanelSectionArg(sectionName),
            arg1: 0
        });
    }

    function postClosePhonePanel() {
        return postJson("/ui-action", {
            type: "CLOSE_PHONE_PANEL",
            targetId: 0,
            arg0: 0,
            arg1: 0
        });
    }

    function cancelCurrentSiteAction() {
        const siteAction = latestState ? latestState.siteAction : null;
        const hasActiveAction =
            !!siteAction &&
            typeof siteAction.actionKind === "number" &&
            siteAction.actionKind !== 0 &&
            typeof siteAction.flags === "number" &&
            (siteAction.flags & 1) !== 0;
        if (!hasActiveAction) {
            return Promise.resolve(false);
        }

        return postSiteActionCancel({
            actionId: typeof siteAction.actionId === "number" ? siteAction.actionId : 0,
            flags: siteActionCancelFlags.currentAction
        }).then(() => true).catch(() => {
            statusChip.textContent = "Failed to cancel current action.";
            return true;
        });
    }

    function closeTrackedSitePanelLayer(state) {
        if (!state || state.appState !== "SITE_ACTIVE") {
            return Promise.resolve(false);
        }

        const protectionSelectorSetup = getSiteProtectionSelectorSetup(state);
        if (protectionSelectorSetup) {
            return postCloseSiteProtectionUi().then(() => true).catch(() => {
                statusChip.textContent = "Failed to close protection selector.";
                return true;
            });
        }

        const techTreeSetup = getTechTreeSetup(state);
        if (techTreeSetup) {
            const closeAction = getTechTreeCloseAction(techTreeSetup);
            if (closeAction) {
                return postJson("/ui-action", closeAction).then(() => true).catch(() => {
                    statusChip.textContent = "Failed to close tech-tree panel.";
                    return true;
                });
            }
        }

        if (isPhonePanelOpen(state)) {
            return postClosePhonePanel().then(() => true).catch(() => {
                statusChip.textContent = "Failed to close phone panel.";
                return true;
            });
        }

        const openedContainer = findOpenedInventoryContainer(state);
        if (openedContainer && typeof openedContainer.storageId === "number") {
            return postJson("/site-storage-view", {
                storageId: openedContainer.storageId,
                eventKind: "CLOSE"
            }).then(() => true).catch(() => {
                statusChip.textContent = "Failed to close storage.";
                return true;
            });
        }

        if (isWorkerPackPanelOpen(state)) {
            return postWorkerPackPanelEvent("CLOSE").then(() => true).catch(() => {
                statusChip.textContent = "Failed to close player pack.";
                return true;
            });
        }

        return Promise.resolve(false);
    }

    function postPhoneTaskAction(type, task) {
        return postJson("/ui-action", {
            type: type,
            targetId: task.taskInstanceId,
            arg0: task.taskTemplateId || 0,
            arg1: 0
        });
    }

    function makePhoneStepperButton(label, onClick, disabled) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "phone-stepper-button";
        button.textContent = label;
        button.disabled = !!disabled;
        bindReliablePrimaryPress(button, onClick);
        return button;
    }

    function makePhoneBackButton(targetSectionName) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "phone-back-button";
        button.textContent = "Back";
        bindReliablePrimaryPress(button, function () {
            postPhonePanelSection(targetSectionName).catch(() => {
                statusChip.textContent = "Failed to switch phone panel section.";
            });
        });
        return button;
    }

    function makePhoneCartButton(cartItemCount) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "phone-cart-button";

        const glyph = document.createElement("span");
        glyph.className = "phone-cart-icon";
        glyph.innerHTML =
            '<span class="phone-cart-icon-basket"></span>' +
            '<span class="phone-cart-icon-handle"></span>';
        button.appendChild(glyph);

        const label = document.createElement("span");
        label.className = "phone-cart-button-label";
        label.textContent = "Cart";
        button.appendChild(label);

        if (cartItemCount > 0) {
            const badge = document.createElement("span");
            badge.className = "phone-cart-count-badge";
            badge.textContent = String(cartItemCount);
            button.appendChild(badge);
        }

        bindReliablePrimaryPress(button, function () {
            postPhonePanelSection("CART").catch(() => {
                statusChip.textContent = "Failed to switch phone panel section.";
            });
        });
        return button;
    }

    function makePhoneAppLauncher(title, onClick) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "phone-app-launcher phone-app-launcher-simple";
        const visual = phoneAppVisuals[title] || { iconKey: "compass", light: "#8f7a64", dark: "#5a4837" };
        button.style.setProperty("--launcher-icon-light", visual.light);
        button.style.setProperty("--launcher-icon-dark", visual.dark);

        const icon = document.createElement("div");
        icon.className = "phone-app-launcher-icon";
        appendUiIcon(icon, visual.iconKey);
        button.appendChild(icon);

        const name = document.createElement("div");
        name.className = "phone-app-launcher-name";
        name.textContent = title;
        button.appendChild(name);

        bindReliablePrimaryPress(button, onClick);
        return button;
    }

    function appendPhonePanelToolbar(container, buttons) {
        const toolbar = document.createElement("div");
        toolbar.className = "phone-panel-toolbar";
        buttons.forEach(function (button) {
            if (button) {
                toolbar.appendChild(button);
            }
        });
        container.appendChild(toolbar);
    }

    function appendPhoneBuyControls(card, listing) {
        const controls = document.createElement("div");
        controls.className = "phone-stepper-row";

        controls.appendChild(
            makePhoneStepperButton(
                "-",
                function () {
                    postPhoneCartAction("REMOVE_PHONE_LISTING_FROM_CART", listing).catch(() => {
                        statusChip.textContent = "Failed to remove item from cart.";
                    });
                },
                (listing.cartQuantity || 0) <= 0
            )
        );

        const count = document.createElement("div");
        count.className = "phone-cart-inline-count";
        count.textContent = "In Cart " + String(listing.cartQuantity || 0);
        controls.appendChild(count);

        controls.appendChild(
            makePhoneStepperButton(
                "+",
                function () {
                    postPhoneCartAction("ADD_PHONE_LISTING_TO_CART", listing).catch(() => {
                        statusChip.textContent = "Failed to add item to cart.";
                    });
                },
                !canAddPhoneListingToCart(listing)
            )
        );

        card.appendChild(controls);
    }

    function appendPhoneListingSection(container, title, metaText, listings, state, scrollKey, emptyText) {
        const section = makePhoneSection(title, metaText);
        if (listings.length === 0) {
            const emptyState = document.createElement("div");
            emptyState.className = "phone-empty-state";
            emptyState.textContent = emptyText;
            section.appendChild(emptyState);
            container.appendChild(section);
            return;
        }

        const content = appendPhoneSectionContent(section, true);
        bindPhoneScrollableRegion(content, scrollKey);
        const stack = document.createElement("div");
        stack.className = "phone-list-stack";
        content.appendChild(stack);

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

            if (listing.listingKind === "BUY_ITEM") {
                appendPhoneBuyControls(card, listing);
            } else {
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
            }

            stack.appendChild(card);
        });

        container.appendChild(section);
    }

    function appendPhoneCartSection(container, state) {
        const cartListings = getPhoneCartListings(state);
        const cartItemCount = getPhoneCartItemCount(state);
        const subtotal = getPhoneCartSubtotal(state);
        const totalCost = subtotal + phoneDeliveryFee;
        const affordable = canCheckoutPhoneCart(state);
        const section = makePhoneSection(
            "Cart",
            cartItemCount > 0
                ? ("Items " + cartItemCount + "  |  Goods $" + formatMoney(subtotal) + "  |  Total $" + formatMoney(totalCost))
                : "Your cart is empty."
        );

        if (cartListings.length === 0) {
            const emptyState = document.createElement("div");
            emptyState.className = "phone-empty-state";
            emptyState.textContent = "Add marketplace items with + to prepare a delivery order.";
            section.appendChild(emptyState);
            container.appendChild(section);
            return;
        }

        const stack = document.createElement("div");
        stack.className = "phone-list-stack";
        section.appendChild(stack);

        cartListings.forEach((listing) => {
            const card = document.createElement("article");
            card.className = "phone-list-card";

            const headerRow = document.createElement("div");
            headerRow.className = "phone-list-header";

            const titleElement = document.createElement("div");
            titleElement.className = "phone-list-title";
            titleElement.textContent = getPhoneListingTitle(listing);
            headerRow.appendChild(titleElement);

            const badge = document.createElement("div");
            badge.className = "phone-list-badge";
            badge.textContent = "Cart";
            headerRow.appendChild(badge);
            card.appendChild(headerRow);

            const metaElement = document.createElement("div");
            metaElement.className = "phone-list-meta";
            metaElement.textContent =
                "$" + formatMoney(listing.price) +
                "  |  qty x" + (listing.cartQuantity || 0) +
                "  |  line $" + formatMoney((listing.price || 0) * (listing.cartQuantity || 0));
            card.appendChild(metaElement);

            appendPhoneBuyControls(card, listing);
            stack.appendChild(card);
        });

        const footer = document.createElement("div");
        footer.className = "phone-cart-footer";

        const fee = document.createElement("div");
        fee.className = "phone-cart-fee";
        fee.textContent = "Delivery Fee $" + formatMoney(phoneDeliveryFee) + "  |  Total $" + formatMoney(totalCost);
        footer.appendChild(fee);

        footer.appendChild(
            makePhoneActionButton(
                "Buy For $" + formatMoney(totalCost),
                function () {
                    postPhoneCartCheckout().catch(() => {
                        statusChip.textContent = "Failed to submit the cart checkout.";
                    });
                },
                true,
                !affordable
            )
        );

        section.appendChild(footer);
        container.appendChild(section);
    }

    function getPhoneTaskStateLabel(task) {
        switch (task.listKind) {
        case "ACCEPTED":
            return "Active";
        case "COMPLETED":
            return "Done";
        case "CLAIMED":
            return "History";
        case "VISIBLE":
        default:
            return "Open";
        }
    }

    function getPhoneTaskStateClassName(task) {
        let className = "phone-task-state";
        if (task.listKind === "ACCEPTED") {
            className += " accepted";
        } else if (task.listKind === "COMPLETED" || task.listKind === "CLAIMED") {
            className += " completed";
        }
        return className;
    }

    function getPhoneTaskTemplatePresentation(task) {
        switch (task.taskTemplateId) {
        case 101:
            return {
                title: "Lay One Checkerboard",
                summary: "Plant 1 Basic Straw Checkerboard to learn the first placement step.",
                reward: "Reward: 3 Wood, 2 Iron, and +10 total reputation."
            };
        case 102:
            return {
                title: "Craft a Storage Crate",
                summary: "Use the starter workbench to craft 1 Storage Crate Kit.",
                reward: "Reward: $8.00 and +10 total reputation."
            };
        case 103:
            return {
                title: "Buy Water",
                summary: "Buy 1 Water from the phone market.",
                reward: "Reward: $10.00 and +10 total reputation."
            };
        case 104:
            return {
                title: "Buy Food",
                summary: "Buy 1 Food from the phone market.",
                reward: "Reward: 2 Wood, 1 Iron, and +10 total reputation."
            };
        case 105:
            return {
                title: "Craft a Shovel",
                summary: "The first unlock just landed. Craft 1 Shovel at the workbench.",
                reward: "Reward: 8 Basic Straw Checkerboards and +10 total reputation."
            };
        case 106:
            return {
                title: "Hold the Starter Patch",
                summary: "Keep the 2 starter Desert Ephedra plants from losing density for 0.5 in-game hour.",
                reward: "Reward: 4 Wood, 2 Iron, and +10 total reputation."
            };
        case 107:
            return {
                title: "Craft a Cook Plot",
                summary: "Craft 1 Camp Stove Kit at the workbench.",
                reward: "Reward: +10 total reputation."
            };
        case 108:
            return {
                title: "Place the Cook Plot",
                summary: "Deploy 1 Camp Stove on the ground beside camp.",
                reward: "Reward: $2.00 and +10 total reputation."
            };
        case 109:
            return {
                title: "Harvest Desert Ephedra",
                summary: "Harvest 1 starter Desert Ephedra patch.",
                reward: "Reward: 1 Water and +10 total reputation."
            };
        case 110:
            return {
                title: "Cook Ephedra Stew",
                summary: "Cook 1 Ephedra Stew from harvested Desert Ephedra Sprigs.",
                reward: "Reward: 2 Desert Ephedra Seeds and +10 total reputation."
            };
        default:
            return {
                title: "Contract " + task.taskInstanceId,
                summary: "Template " + task.taskTemplateId,
                reward: ""
            };
        }
    }

    function appendPhoneTaskAction(card, task) {
        if (task.listKind !== "VISIBLE") {
            return;
        }

        card.appendChild(
            makePhoneActionButton(
                "Accept Contract",
                function () {
                    postPhoneTaskAction("ACCEPT_TASK", task).catch(() => {
                        statusChip.textContent = "Failed to accept contract.";
                    });
                },
                true,
                false
            )
        );
    }

    function appendPhoneTaskSection(container, tasks, metaText, scrollKey) {
        const visibleTaskCount = countTasksByListKind(tasks, "VISIBLE");
        const acceptedTaskCount = countTasksByListKind(tasks, "ACCEPTED");
        const section = makePhoneSection(
            "Contracts",
            metaText || ("Open " + visibleTaskCount + "  |  Active " + acceptedTaskCount)
        );

        if (tasks.length === 0) {
            const emptyState = document.createElement("div");
            emptyState.className = "phone-empty-state";
            emptyState.textContent = "No field contracts are synced to the phone yet.";
            section.appendChild(emptyState);
            container.appendChild(section);
            return;
        }

        const content = appendPhoneSectionContent(section, true);
        bindPhoneScrollableRegion(content, scrollKey || "contracts");
        const stack = document.createElement("div");
        stack.className = "phone-list-stack";
        content.appendChild(stack);

        tasks.forEach((task) => {
            const taskPresentation = getPhoneTaskTemplatePresentation(task);
            const targetProgress = Math.max(task.targetProgress || 0, 1);
            const currentProgress = Math.max(0, Math.min(task.currentProgress || 0, targetProgress));
            const completion = Math.round((currentProgress / targetProgress) * 100);
            const card = document.createElement("article");
            card.className = "phone-task-card";

            const header = document.createElement("div");
            header.className = "phone-task-header";

            const titleElement = document.createElement("div");
            titleElement.className = "phone-task-title";
            titleElement.textContent = taskPresentation.title;
            header.appendChild(titleElement);

            const stateElement = document.createElement("div");
            stateElement.className = getPhoneTaskStateClassName(task);
            stateElement.textContent = getPhoneTaskStateLabel(task);
            header.appendChild(stateElement);
            card.appendChild(header);

            const metaElement = document.createElement("div");
            metaElement.className = "phone-task-meta";
            metaElement.textContent =
                taskPresentation.summary +
                "  |  Progress " + currentProgress + "/" + targetProgress +
                "  |  " + completion + "%";
            card.appendChild(metaElement);

            if (taskPresentation.reward) {
                const rewardElement = document.createElement("div");
                rewardElement.className = "phone-task-meta";
                rewardElement.textContent = taskPresentation.reward;
                card.appendChild(rewardElement);
            }

            const track = document.createElement("div");
            track.className = "phone-task-track";
            const fill = document.createElement("div");
            fill.className = "phone-task-fill";
            fill.style.width = completion + "%";
            track.appendChild(fill);
            card.appendChild(track);

            appendPhoneTaskAction(card, task);

            stack.appendChild(card);
        });

        container.appendChild(section);
    }

    function setPhoneHeader(title, subtitle) {
        phoneAppTitle.textContent = title;
        phoneAppSubtitle.hidden = !subtitle;
        phoneAppSubtitle.textContent = subtitle || "";
    }

    function setPhoneHeaderForSection(activeSection) {
        switch (activeSection) {
        case "TASKS":
            setPhoneHeader("Tasks", "");
            break;
        case "BUY":
            setPhoneHeader("Buy", "");
            break;
        case "SELL":
            setPhoneHeader("Sell", "");
            break;
        case "HIRE":
            setPhoneHeader("Services", "");
            break;
        case "CART":
            setPhoneHeader("Cart", "Review the delivery order before checkout. Back returns to Buy.");
            break;
        case "HOME":
        default:
            setPhoneHeader("Apps", "");
            break;
        }
    }

    function appendPhoneHomeScreen(container) {
        const appGrid = document.createElement("div");
        appGrid.className = "phone-home-grid";
        appGrid.appendChild(
            makePhoneAppLauncher(
                "Tasks",
                function () {
                    postPhonePanelSection("TASKS").catch(() => {
                        statusChip.textContent = "Failed to switch phone panel section.";
                    });
                }
            )
        );
        appGrid.appendChild(
            makePhoneAppLauncher(
                "Buy",
                function () {
                    postPhonePanelSection("BUY").catch(() => {
                        statusChip.textContent = "Failed to switch phone panel section.";
                    });
                }
            )
        );
        appGrid.appendChild(
            makePhoneAppLauncher(
                "Sell",
                function () {
                    postPhonePanelSection("SELL").catch(() => {
                        statusChip.textContent = "Failed to switch phone panel section.";
                    });
                }
            )
        );
        appGrid.appendChild(
            makePhoneAppLauncher(
                "Services",
                function () {
                    postPhonePanelSection("HIRE").catch(() => {
                        statusChip.textContent = "Failed to switch phone panel section.";
                    });
                }
            )
        );
        container.appendChild(appGrid);
    }

    function appendPhoneBuyPanel(container, state, buyListings, buyListingCount, cartItemCount) {
        appendPhonePanelToolbar(container, [makePhoneBackButton("HOME"), makePhoneCartButton(cartItemCount)]);
        const marketplaceSection = makePhoneSection(
            "Marketplace",
            "Listings " + buyListingCount + "  |  Cart " + cartItemCount
        );

        if (buyListings.length === 0) {
            const emptyMarket = document.createElement("div");
            emptyMarket.className = "phone-empty-state";
            emptyMarket.textContent = "No buy listings are available right now.";
            marketplaceSection.appendChild(emptyMarket);
            container.appendChild(marketplaceSection);
            return;
        }

        const content = appendPhoneSectionContent(marketplaceSection, true);
        bindPhoneScrollableRegion(content, "buy-marketplace");
        const stack = document.createElement("div");
        stack.className = "phone-list-stack";
        content.appendChild(stack);

        buyListings.forEach((listing) => {
            const card = document.createElement("article");
            card.className = "phone-list-card";

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
            metaElement.textContent = getPhoneListingMetaText(listing, canAddPhoneListingToCart(listing));
            card.appendChild(metaElement);

            appendPhoneBuyControls(card, listing);
            stack.appendChild(card);
        });

        container.appendChild(marketplaceSection);
    }

    function appendPhoneSellPanel(container, state, sellListings, sellListingCount) {
        appendPhonePanelToolbar(container, [makePhoneBackButton("HOME")]);
        appendPhoneListingSection(
            container,
            "Sellback",
            "Listings " + sellListingCount + "  |  Current inventory surfaced for quick sale",
            sellListings,
            state,
            "sellback",
            "No sell listings are available right now."
        );
    }

    function appendPhoneServicePanel(container, state, specialListings, serviceListingCount) {
        appendPhonePanelToolbar(container, [makePhoneBackButton("HOME")]);
        appendPhoneListingSection(
            container,
            "Services",
            "Listings " + serviceListingCount + "  |  Remote services and unlockables",
            specialListings,
            state,
            "services",
            "No remote services are available right now."
        );
    }

    function appendPhoneTasksPanel(container, tasks, activeTaskCount, visibleTaskCount) {
        appendPhonePanelToolbar(container, [makePhoneBackButton("HOME")]);
        appendPhoneTaskSection(
            container,
            tasks,
            "Open " + visibleTaskCount + "  |  Active " + activeTaskCount,
            "contracts"
        );
    }

    function appendPhoneCartPanel(container, state) {
        appendPhonePanelToolbar(container, [makePhoneBackButton("BUY")]);
        appendPhoneCartSection(container, state);
    }

    function renderPhonePanel(state) {
        const isSiteActive = !!state && state.appState === "SITE_ACTIVE";
        phonePanelOpen = isPhonePanelOpen(state);
        syncPhonePanelVisibility(isSiteActive);
        capturePhoneScrollPositions();
        phoneScreenBody.innerHTML = "";

        if (!isSiteActive || !phonePanelOpen) {
            return;
        }

        phoneStatusTime.textContent = formatPhoneClockLabel();

        const phonePanelState = getPhonePanelState(state);
        const tasks = getSiteTasks(state);
        const buyListings = getBuyListings(state);
        const sellListings = getSellListings(state);
        const specialListings = getSpecialPhoneListings(state);
        const cartItemCount = getPhoneCartItemCount(state);
        const activeSection =
            phonePanelState && typeof phonePanelState.activeSection === "string"
                ? phonePanelState.activeSection
                : "HOME";
        const activeTaskCount = phonePanelState && typeof phonePanelState.acceptedTaskCount === "number"
            ? phonePanelState.acceptedTaskCount
            : countTasksByListKind(tasks, "ACCEPTED");
        const visibleTaskCount = phonePanelState && typeof phonePanelState.visibleTaskCount === "number"
            ? phonePanelState.visibleTaskCount
            : countTasksByListKind(tasks, "VISIBLE");
        const buyListingCount = phonePanelState && typeof phonePanelState.buyListingCount === "number"
            ? phonePanelState.buyListingCount
            : buyListings.length;
        const sellListingCount = phonePanelState && typeof phonePanelState.sellListingCount === "number"
            ? phonePanelState.sellListingCount
            : sellListings.length;
        const serviceListingCount = phonePanelState && typeof phonePanelState.serviceListingCount === "number"
            ? phonePanelState.serviceListingCount
            : specialListings.length;

        setPhoneHeaderForSection(activeSection);

        switch (activeSection) {
        case "TASKS":
            appendPhoneTasksPanel(phoneScreenBody, tasks, activeTaskCount, visibleTaskCount);
            break;
        case "BUY":
            appendPhoneBuyPanel(phoneScreenBody, state, buyListings, buyListingCount, cartItemCount);
            break;
        case "SELL":
            appendPhoneSellPanel(phoneScreenBody, state, sellListings, sellListingCount);
            break;
        case "HIRE":
            appendPhoneServicePanel(phoneScreenBody, state, specialListings, serviceListingCount);
            break;
        case "CART":
            appendPhoneCartPanel(phoneScreenBody, state);
            break;
        case "HOME":
        default:
            appendPhoneHomeScreen(phoneScreenBody);
            break;
        }

        restorePhoneScrollPositions();
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
        const menuSetup = getSetup(state, "REGIONAL_MAP_MENU");
        const selectionSetup = getSetup(state, "REGIONAL_MAP_SELECTION");
        const techTreeSetup = getTechTreeSetup(state);
        const labels = getLabelElements(selectionSetup);
        const actions = getVisibleActionElements(selectionSetup);
        const menuActions = getVisibleActionElements(menuSetup);
        const primaryLabel = labels.length > 0 ? labels[0].text : "";

        menuPanel.hidden = true;

        if (techTreeSetup) {
            selectionEyebrow.textContent = "Campaign Research";
            selectionText.hidden = true;
            selectionText.textContent = "";
            renderTechTreePanel(techTreeSetup, {
                kickerText: "Regional Research Board"
            });
            renderStoragePanel(null, null);
            contextActions.innerHTML = "";
            return;
        }

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
        menuActions.forEach((element) => {
            contextActions.appendChild(
                makeButton(
                    element.text || element.action.type,
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
        selectionChip.hidden = false;
        selectionEyebrow.textContent = "Current View";
        selectionText.hidden = false;
        selectionText.textContent =
            siteBootstrap
                ? "Site " + siteBootstrap.siteId + " placeholder view."
                : "Waiting for the next presentation state.";
        selectionInventory.hidden = true;
        selectionInventory.innerHTML = "";
        renderStoragePanel(null, null);
        contextActions.innerHTML = "";
    }

    function renderSiteResultOverlay(state) {
        const resultSetup = getSetup(state, "SITE_RESULT");
        const labels = getLabelElements(resultSetup);
        const actions = getVisibleActionElements(resultSetup);
        const siteResult = getSiteResult(state);
        const primaryLabel = labels.length > 0 ? labels[0].text : "";
        const resultCompleted = !!siteResult && siteResult.result === "COMPLETED";
        const resultLine =
            primaryLabel ||
            (siteResult
                ? ("Site " + siteResult.siteId + " " + (resultCompleted ? "completed" : "failed"))
                : "Site result ready");
        const revealCount =
            siteResult && typeof siteResult.newlyRevealedSiteCount === "number"
                ? siteResult.newlyRevealedSiteCount
                : 0;

        menuPanel.hidden = true;
        selectionChip.hidden = false;
        selectionEyebrow.textContent = resultCompleted ? "Mission Success" : "Mission Failed";
        selectionText.hidden = false;
        selectionText.textContent = resultLine;
        selectionInventory.classList.remove("site-result-actions");
        selectionInventory.hidden = false;
        selectionInventory.innerHTML = "";
        renderStoragePanel(null, null);
        contextActions.innerHTML = "";
        actionProgress.hidden = true;
        statusChip.textContent = "";

        const summary = document.createElement("div");
        summary.className = "site-result-summary";
        summary.textContent =
            revealCount > 0
                ? ("Nearby survey updated. " + revealCount + " new route" + (revealCount === 1 ? " is" : "s are") + " now visible on the regional map.")
                : "Field report logged. Confirm the result to return to the regional survey.";
        selectionInventory.appendChild(summary);

        const actionGroup = document.createElement("div");
        actionGroup.className = "site-result-actions";
        selectionInventory.appendChild(actionGroup);

        actions.forEach((element) => {
            let label = element.text || element.action.type;
            if (element.action && element.action.type === "RETURN_TO_REGIONAL_MAP") {
                label = "OK";
            }

            actionGroup.appendChild(
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

    function protectionOverlayPresentation(modeName) {
        switch (modeName) {
        case "WIND":
            return { title: "Wind Protection", iconKey: "wind" };
        case "HEAT":
            return { title: "Heat Protection", iconKey: "flame" };
        case "DUST":
            return { title: "Dust Protection", iconKey: "grid" };
        default:
            return { title: "Protection", iconKey: "compass" };
        }
    }

    function renderProtectionSelectorPanel(setup) {
        const selectorActions = getVisibleActionElements(setup);
        selectionChip.hidden = false;
        selectionInventory.hidden = false;
        selectionInventory.innerHTML = "";
        contextActions.innerHTML = "";

        const panel = document.createElement("section");
        panel.className = "protection-selector-panel";
        selectionInventory.appendChild(panel);

        const title = document.createElement("div");
        title.className = "protection-selector-title";
        title.textContent = "Protection Overlay";
        panel.appendChild(title);

        const copy = document.createElement("div");
        copy.className = "protection-selector-copy";
        copy.textContent = "Pick one channel to compare final per-tile shelter on a strict 0-100 scale.";
        panel.appendChild(copy);

        const grid = document.createElement("div");
        grid.className = "protection-selector-grid";
        panel.appendChild(grid);

        selectorActions.forEach((element) => {
            const button = document.createElement("button");
            button.type = "button";
            button.className = "protection-selector-button";

            const modeName =
                element.action && typeof element.action.arg0 === "number"
                    ? (
                        element.action.arg0 === protectionOverlayModes.wind ? "WIND" :
                        element.action.arg0 === protectionOverlayModes.heat ? "HEAT" :
                        element.action.arg0 === protectionOverlayModes.dust ? "DUST" :
                        "NONE")
                    : "NONE";
            const presentation = protectionOverlayPresentation(modeName);

            bindReliablePrimaryPress(button, function () {
                postJson("/ui-action", element.action).catch(() => {
                    statusChip.textContent = "Failed to enter protection overlay.";
                });
            });

            const icon = document.createElement("div");
            icon.className = "protection-selector-icon";
            appendUiIcon(icon, presentation.iconKey);
            button.appendChild(icon);

            const titleElement = document.createElement("div");
            titleElement.className = "protection-selector-button-title";
            titleElement.textContent = presentation.title;
            button.appendChild(titleElement);

            const subtitle = document.createElement("div");
            subtitle.className = "protection-selector-button-subtitle";
            subtitle.textContent = "0 red  100 green";
            button.appendChild(subtitle);

            grid.appendChild(button);
        });
    }

    function renderSiteOverlay(state) {
        const protectionSelectorSetup = getSiteProtectionSelectorSetup(state);
        const techTreeSetup = getTechTreeSetup(state);
        const workerPackSlots = getInventorySlotsByKind(state, "WORKER_PACK");

        menuPanel.hidden = true;
        selectionChip.hidden = false;
        contextActions.innerHTML = "";
        clearSelectedInventorySlotIfInvalid(state);
        clearOpenedInventoryContainerIfInvalid(state);

        if (protectionSelectorSetup) {
            selectionEyebrow.textContent = "Site Analysis";
            selectionText.hidden = true;
            selectionText.textContent = "";
            renderProtectionSelectorPanel(protectionSelectorSetup);
            renderStoragePanel(null, null);
            return;
        }

        if (techTreeSetup) {
            selectionEyebrow.textContent = "Campaign Research";
            selectionText.hidden = true;
            selectionText.textContent = "";
            renderTechTreePanel(techTreeSetup, {
                kickerText: "Site Session Research"
            });
            renderStoragePanel(null, null);
            return;
        }

        const selectedSlot = findSelectedInventorySlot(state);
        const openedContainerInfo = findOpenedInventoryContainer(state);
        selectionEyebrow.textContent = "Player Pack";
        selectionText.hidden = true;
        selectionText.textContent = "";
        renderSiteInventoryPanel(state, workerPackSlots);
        renderStoragePanel(state, openedContainerInfo);
        appendSiteTechTreeToggleAction(techTreeSetup);
        appendSelectedInventoryActions(state, selectedSlot);
    }

    function updateOverlay(state, options) {
        const renderOptions = options || {};
        stageFrame.classList.toggle("main-menu-mode", state.appState === "MAIN_MENU");
        stageFrame.classList.toggle("regional-map-mode", state.appState === "REGIONAL_MAP");
        stageFrame.classList.toggle("site-active-mode", state.appState === "SITE_ACTIVE");
        stageFrame.classList.toggle("site-result-mode", state.appState === "SITE_RESULT");
        setTechTreeOverlayActive(false);
        stageFrame.classList.toggle("protection-selector-active", !!getSiteProtectionSelectorSetup(state));
        const appStateChanged = lastOverlayAppState !== state.appState;

        switch (state.appState) {
        case "MAIN_MENU":
            hudEyebrow.textContent = "App State";
            hudTitle.textContent = state.appState || "NONE";
            hudSubtitle.textContent = "Austere, painterly, and severe: the campaign opens under hostile conditions.";
            renderSiteVitalsPanel(null);
            inventoryPanelOpen = true;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            selectionChip.hidden = false;
            selectionText.hidden = false;
            clearSelectionInventory();
            renderMenuOverlay(state);
            statusChip.textContent = "Prototype Build\nVisual Smoke";
            break;
        case "REGIONAL_MAP":
            hudEyebrow.textContent = "App State";
            hudTitle.textContent = state.appState || "NONE";
            hudSubtitle.textContent = "Review the campaign survey board and choose the next deployment route.";
            renderSiteVitalsPanel(state);
            inventoryPanelOpen = true;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            selectionChip.hidden = false;
            selectionText.hidden = false;
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
                statusChip.textContent = viewerCompatibilityWarning || "";
            }
            inventoryPanelOpen = isWorkerPackPanelOpen(state);
            phonePanelOpen = isPhonePanelOpen(state);
            renderSiteHudChrome(state);
            renderSiteOverlay(state);
            break;
        case "SITE_RESULT":
            hudEyebrow.textContent = "App State";
            hudTitle.textContent = state.appState || "NONE";
            hudSubtitle.textContent = "The field report is in. Confirm the result and return to the regional survey.";
            renderSiteVitalsPanel(null);
            inventoryPanelOpen = false;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            selectionChip.hidden = false;
            selectionText.hidden = false;
            clearSelectionInventory();
            renderSiteResultOverlay(state);
            break;
        default:
            hudEyebrow.textContent = "App State";
            hudTitle.textContent = state.appState || "NONE";
            hudSubtitle.textContent = "The current adapter only styles the core early flow for now.";
            renderSiteVitalsPanel(null);
            inventoryPanelOpen = true;
            phonePanelOpen = false;
            selectedInventorySlotKey = "";
            openedInventoryContainerKey = "";
            selectionChip.hidden = false;
            selectionText.hidden = false;
            clearSelectionInventory();
            renderFallbackOverlay(state);
            statusChip.textContent =
                "Connected\nFrame " + state.frameNumber +
                "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            break;
        }

        if (state.appState !== "SITE_ACTIVE" ||
            !phonePanelOpen ||
            !isPhoneInteractionLocked() ||
            renderOptions.forcePhonePanelRender) {
            renderPhonePanel(state);
        }
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

    function createRegionalMapBoardVisual() {
        const boardGroup = new THREE_NS.Group();

        const board = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(34.0, 0.72, 22.0),
            new THREE_NS.MeshStandardMaterial({ color: 0x6c4b2d, roughness: 0.95, metalness: 0.04 })
        );
        board.position.set(0, -0.48, 0);
        boardGroup.add(board);

        const mapSheet = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(31.0, 0.16, 18.8),
            new THREE_NS.MeshStandardMaterial({ color: 0xd9bf90, roughness: 0.98, metalness: 0.01 })
        );
        mapSheet.position.set(0, -0.03, 0);
        boardGroup.add(mapSheet);

        const mapInset = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(29.8, 0.04, 17.6),
            new THREE_NS.MeshStandardMaterial({ color: 0xcfae78, roughness: 1.0, metalness: 0.0 })
        );
        mapInset.position.set(0, 0.06, 0);
        boardGroup.add(mapInset);

        const northMarker = new THREE_NS.Mesh(
            new THREE_NS.ConeGeometry(0.18, 0.6, 3),
            new THREE_NS.MeshStandardMaterial({ color: 0x53351b, roughness: 0.86, metalness: 0.05 })
        );
        northMarker.rotation.x = Math.PI;
        northMarker.position.set(13.5, 0.28, -7.6);
        boardGroup.add(northMarker);

        return collapseStaticMeshHierarchy(boardGroup);
    }

    function createRegionalMapTileVisual(tileSpacing, occupied, positionX, positionZ) {
        const tileGroup = new THREE_NS.Group();

        const tile = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(tileSpacing * 0.86, 0.06, tileSpacing * 0.86),
            new THREE_NS.MeshStandardMaterial({
                color: occupied ? 0xcfa66e : 0xdbbd8c,
                roughness: occupied ? 0.92 : 0.97,
                metalness: 0.01
            })
        );
        tile.position.set(positionX, 0.1, positionZ);
        tileGroup.add(tile);

        const tileOutline = new THREE_NS.Mesh(
            new THREE_NS.BoxGeometry(tileSpacing * 0.92, 0.01, tileSpacing * 0.92),
            new THREE_NS.MeshStandardMaterial({
                color: occupied ? 0x8e6639 : 0xb98d5b,
                roughness: 1.0,
                metalness: 0.0
            })
        );
        tileOutline.position.set(positionX, 0.135, positionZ);
        tileGroup.add(tileOutline);

        return collapseStaticMeshHierarchy(tileGroup);
    }

    function createRegionalMapMarkerVisual(site, isSelected) {
        const markerGroup = new THREE_NS.Group();
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

        markerGroup.userData = { siteId: site.siteId, siteState: site.siteState };
        return collapseStaticMeshHierarchy(markerGroup);
    }

    function renderRegionalMapScene(state) {
        currentSceneKind = "REGIONAL_MAP";
        scene.background = new THREE_NS.Color(0xe6cfac);

        worldGroup.add(createRegionalMapBoardVisual());

        const positions = new Map();
        const mapTileSourceSpacing = 160;
        const scale = 0.03;
        const tileSpacing = mapTileSourceSpacing * scale;
        const rawPositions = state.regionalMap.sites.map((site) => ({
            siteId: site.siteId,
            tileX: Math.round(site.mapX / mapTileSourceSpacing),
            tileY: Math.round(site.mapY / mapTileSourceSpacing),
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

            rawPositions.forEach((entry) => {
                positions.set(entry.siteId, {
                    x: entry.rawX - rawCenterX,
                    z: entry.rawZ - rawCenterZ
                });
            });

            const tileXs = rawPositions.map((entry) => entry.tileX);
            const tileYs = rawPositions.map((entry) => entry.tileY);
            const minTileX = Math.min.apply(null, tileXs) - 1;
            const maxTileX = Math.max.apply(null, tileXs) + 1;
            const minTileY = Math.min.apply(null, tileYs) - 1;
            const maxTileY = Math.max.apply(null, tileYs) + 1;

            for (let tileY = minTileY; tileY <= maxTileY; tileY += 1) {
                for (let tileX = minTileX; tileX <= maxTileX; tileX += 1) {
                    const occupied = rawPositions.some((entry) => entry.tileX === tileX && entry.tileY === tileY);
                    worldGroup.add(
                        createRegionalMapTileVisual(
                            tileSpacing,
                            occupied,
                            tileX * tileSpacing - rawCenterX,
                            -(tileY * tileSpacing) - rawCenterZ
                        )
                    );
                }
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
            const isSelected = state.selectedSiteId === site.siteId || (site.flags & 1) !== 0;
            const markerVisual = createRegionalMapMarkerVisual(site, isSelected);
            markerVisual.position.set(position.x, 0.0, position.z);
            if (site.siteState === "AVAILABLE") {
                mapPickables.push(markerVisual);
            }
            worldGroup.add(markerVisual);
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

    function tagMergeableMaterial(material, traits) {
        if (!material) {
            return material;
        }
        material.userData = material.userData || {};
        if (traits) {
            Object.assign(material.userData, traits);
        }
        return material;
    }

    function getMergeableMaterialSignature(material) {
        if (!material) {
            return "";
        }
        const userData = material.userData || {};
        return JSON.stringify({
            type: material.type || "Material",
            color:
                material.color && typeof material.color.getHex === "function"
                    ? material.color.getHex()
                    : null,
            roughness:
                typeof material.roughness === "number"
                    ? Number(material.roughness.toFixed(4))
                    : null,
            metalness:
                typeof material.metalness === "number"
                    ? Number(material.metalness.toFixed(4))
                    : null,
            emissive:
                material.emissive && typeof material.emissive.getHex === "function"
                    ? material.emissive.getHex()
                    : null,
            emissiveIntensity:
                typeof material.emissiveIntensity === "number"
                    ? Number(material.emissiveIntensity.toFixed(4))
                    : null,
            transparent: material.transparent === true,
            opacity:
                typeof material.opacity === "number"
                    ? Number(material.opacity.toFixed(4))
                    : 1,
            depthWrite: material.depthWrite !== false,
            side:
                typeof material.side === "number"
                    ? material.side
                    : THREE_NS.FrontSide,
            windReactive: userData.windReactive === true,
            windFlexibility:
                typeof userData.windFlexibility === "number"
                    ? Number(userData.windFlexibility.toFixed(4))
                    : null
        });
    }

    function mergeGeometryAttributes(attributes) {
        if (!attributes || attributes.length === 0) {
            return null;
        }

        const first = attributes[0];
        const TypedArray = first.array.constructor;
        const itemSize = first.itemSize;
        const normalized = first.normalized;
        let totalLength = 0;

        for (let index = 0; index < attributes.length; index += 1) {
            const attribute = attributes[index];
            if (!attribute ||
                attribute.array.constructor !== TypedArray ||
                attribute.itemSize !== itemSize ||
                attribute.normalized !== normalized) {
                return null;
            }
            totalLength += attribute.array.length;
        }

        const mergedArray = new TypedArray(totalLength);
        let offset = 0;
        for (let index = 0; index < attributes.length; index += 1) {
            const attribute = attributes[index];
            mergedArray.set(attribute.array, offset);
            offset += attribute.array.length;
        }

        return new THREE_NS.BufferAttribute(mergedArray, itemSize, normalized);
    }

    function mergeCompatibleGeometries(geometries, useGroups) {
        if (!geometries || geometries.length === 0) {
            return null;
        }

        const first = geometries[0];
        const attributeNames = Object.keys(first.attributes);
        const mergedGeometry = new THREE_NS.BufferGeometry();
        const firstHasIndex = first.index !== null;
        let groupOffset = 0;

        for (let geometryIndex = 0; geometryIndex < geometries.length; geometryIndex += 1) {
            const geometry = geometries[geometryIndex];
            if (!geometry || (geometry.index !== null) !== firstHasIndex) {
                return null;
            }

            const geometryAttributeNames = Object.keys(geometry.attributes);
            if (geometryAttributeNames.length !== attributeNames.length) {
                return null;
            }

            for (let attributeIndex = 0; attributeIndex < attributeNames.length; attributeIndex += 1) {
                const attributeName = attributeNames[attributeIndex];
                if (!geometry.getAttribute(attributeName)) {
                    return null;
                }
            }

            if (useGroups) {
                const positionAttribute = geometry.getAttribute("position");
                if (!positionAttribute) {
                    return null;
                }
                const drawCount = firstHasIndex ? geometry.index.count : positionAttribute.count;
                mergedGeometry.addGroup(groupOffset, drawCount, geometryIndex);
                groupOffset += drawCount;
            }
        }

        if (firstHasIndex) {
            const mergedIndex = [];
            let vertexOffset = 0;
            for (let geometryIndex = 0; geometryIndex < geometries.length; geometryIndex += 1) {
                const geometry = geometries[geometryIndex];
                const positionAttribute = geometry.getAttribute("position");
                for (let indexOffset = 0; indexOffset < geometry.index.count; indexOffset += 1) {
                    mergedIndex.push(geometry.index.getX(indexOffset) + vertexOffset);
                }
                vertexOffset += positionAttribute.count;
            }
            mergedGeometry.setIndex(mergedIndex);
        }

        for (let attributeIndex = 0; attributeIndex < attributeNames.length; attributeIndex += 1) {
            const attributeName = attributeNames[attributeIndex];
            const mergedAttribute = mergeGeometryAttributes(
                geometries.map((geometry) => geometry.getAttribute(attributeName))
            );
            if (!mergedAttribute) {
                return null;
            }
            mergedGeometry.setAttribute(attributeName, mergedAttribute);
        }

        return mergedGeometry;
    }

    function collapseStaticMeshHierarchy(root) {
        if (!root || root.isMesh) {
            return root;
        }

        const meshEntries = [];
        let unsupportedContent = false;
        root.updateMatrixWorld(true);
        const inverseRootWorld = new THREE_NS.Matrix4().copy(root.matrixWorld).invert();

        root.traverse((node) => {
            if (node === root) {
                return;
            }
            if (node.isMesh) {
                if (!node.geometry || !node.material || Array.isArray(node.material)) {
                    unsupportedContent = true;
                    return;
                }
                const geometry = node.geometry.index ? node.geometry.toNonIndexed() : node.geometry.clone();
                const localMatrix = new THREE_NS.Matrix4().multiplyMatrices(
                    inverseRootWorld,
                    node.matrixWorld
                );
                geometry.applyMatrix4(localMatrix);
                meshEntries.push({
                    geometry: geometry,
                    material: node.material,
                    materialSignature: getMergeableMaterialSignature(node.material)
                });
                return;
            }
            if (!node.isGroup && node.type !== "Object3D") {
                unsupportedContent = true;
            }
        });

        if (unsupportedContent || meshEntries.length <= 1) {
            return root;
        }

        const geometryBatches = new Map();
        meshEntries.forEach((entry) => {
            if (!geometryBatches.has(entry.materialSignature)) {
                geometryBatches.set(entry.materialSignature, {
                    material: entry.material,
                    geometries: []
                });
            }
            geometryBatches.get(entry.materialSignature).geometries.push(entry.geometry);
        });

        const mergedGeometries = [];
        const mergedMaterials = [];
        for (const batch of geometryBatches.values()) {
            const mergedGeometry =
                batch.geometries.length === 1
                    ? batch.geometries[0]
                    : mergeCompatibleGeometries(batch.geometries, false);
            if (!mergedGeometry) {
                return root;
            }
            mergedGeometries.push(mergedGeometry);
            mergedMaterials.push(batch.material);
        }

        const finalGeometry =
            mergedGeometries.length === 1
                ? mergedGeometries[0]
                : mergeCompatibleGeometries(mergedGeometries, true);
        if (!finalGeometry) {
            return root;
        }

        const mergedMesh = new THREE_NS.Mesh(
            finalGeometry,
            mergedMaterials.length === 1 ? mergedMaterials[0] : mergedMaterials
        );
        mergedMesh.position.copy(root.position);
        mergedMesh.quaternion.copy(root.quaternion);
        mergedMesh.scale.copy(root.scale);
        mergedMesh.rotation.order = root.rotation.order;
        mergedMesh.visible = root.visible;
        mergedMesh.renderOrder = root.renderOrder;
        mergedMesh.frustumCulled = root.frustumCulled;
        mergedMesh.matrixAutoUpdate = root.matrixAutoUpdate;
        mergedMesh.userData = Object.assign({}, root.userData);
        return mergedMesh;
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

    function createWindFieldVisual(width, height) {
        const group = new THREE_NS.Group();
        const streaks = [];
        const streakCount = Math.max(24, Math.min(64, Math.round((width + height) * 1.15)));
        const horizontalSpan = Math.max(width, height) * 0.82;
        const depthSpan = Math.max(width, height) * 1.04;

        for (let index = 0; index < streakCount; index += 1) {
            const widthScale = 0.42 + ((index * 13) % 11) * 0.055;
            const lengthScale = 0.72 + ((index * 7) % 13) * 0.07;
            const mesh = new THREE_NS.Mesh(
                new THREE_NS.PlaneGeometry(0.13 * widthScale, 1.2 * lengthScale),
                new THREE_NS.MeshBasicMaterial({
                    color: 0xf6edd7,
                    transparent: true,
                    opacity: 0.0,
                    depthWrite: false,
                    blending: THREE_NS.AdditiveBlending,
                    side: THREE_NS.DoubleSide
                })
            );
            mesh.rotation.x = -Math.PI / 2;
            mesh.position.y = 0.84 + (index % 7) * 0.07;
            group.add(mesh);
            streaks.push({
                mesh: mesh,
                progressSeed: ((index * 17) % 37) / 37,
                lateralSeed: (((index % 11) - 5) / 5) + (((index * 19) % 7) - 3) * 0.14,
                lateralDriftSeed: ((index * 29) % 41) / 41,
                lateralDriftPhase: ((index * 31) % 53) / 53,
                verticalSeed: ((index * 11) % 8) * 0.04,
                swaySeed: ((index * 23) % 29) / 29,
                speedScale: 0.72 + ((index * 5) % 9) * 0.09,
                widthScale: widthScale,
                lengthScale: lengthScale,
                spawnCycle: 0,
                active: false,
                nextSpawnAt: (((index * 17) % 37) / 37) * 1.4,
                spawnTime: -1000,
                lifeSeconds: 0,
                streamStartX: 0,
                streamEndX: 0,
                streamY: 0,
                opacityScale: 0,
                streamLengthScale: 1.0,
                streamWidthScale: 1.0,
                travelSpan: depthSpan,
                rotationJitter: 0
            });
        }

        group.visible = false;
        return {
            group: group,
            streaks: streaks,
            horizontalSpan: horizontalSpan,
            depthSpan: depthSpan
        };
    }

    function sampleWindStreakNoise(streak, salt) {
        const value = Math.sin(
            (streak.progressSeed + 0.37) * 157.1 +
            (streak.lateralSeed + 1.41) * 311.7 +
            (streak.spawnCycle + salt) * 73.7 +
            streak.swaySeed * 19.9) * 43758.5453123;
        return value - Math.floor(value);
    }

    function scheduleWindStreakRespawn(streak, elapsedSeconds, minDelaySeconds, maxDelaySeconds) {
        const delayNoise = sampleWindStreakNoise(streak, 0.73);
        streak.active = false;
        streak.nextSpawnAt = elapsedSeconds + lerp(minDelaySeconds, maxDelaySeconds, delayNoise);
        streak.mesh.material.opacity = 0.0;
    }

    function activateWindStreak(streak, windField, elapsedSeconds, windDensityLevel, windSpeedLevel, flowDistance) {
        streak.spawnCycle += 1;
        streak.active = true;
        streak.spawnTime = elapsedSeconds;

        const lateralNoise = sampleWindStreakNoise(streak, 0.11) * 2.0 - 1.0;
        const directionNoise = sampleWindStreakNoise(streak, 0.29) * 2.0 - 1.0;
        const heightNoise = sampleWindStreakNoise(streak, 0.47);
        const lifeNoise = sampleWindStreakNoise(streak, 0.61);
        const opacityNoise = sampleWindStreakNoise(streak, 0.83);
        const spanNoise = sampleWindStreakNoise(streak, 1.07);
        const widthNoise = sampleWindStreakNoise(streak, 1.31);
        const lengthNoise = sampleWindStreakNoise(streak, 1.57);
        const rotationNoise = sampleWindStreakNoise(streak, 1.79) * 2.0 - 1.0;

        streak.lifeSeconds = lerp(4.8, 2.8, windSpeedLevel) * (0.86 + lifeNoise * 0.38);
        streak.travelSpan = flowDistance * (0.8 + spanNoise * 0.3);
        streak.streamStartX = lateralNoise * windField.horizontalSpan * 0.58;
        streak.streamEndX =
            streak.streamStartX +
            directionNoise * streak.travelSpan * lerp(0.015, 0.06, windDensityLevel);
        streak.streamY = 0.66 + heightNoise * 0.56 + windDensityLevel * 0.12;
        streak.opacityScale = 0.45 + opacityNoise * 0.55;
        streak.streamWidthScale = 0.82 + widthNoise * 0.34;
        streak.streamLengthScale = 0.78 + lengthNoise * 0.5;
        streak.rotationJitter = rotationNoise * 0.045;
    }

    function updateWindFieldVisual(cache, state, elapsedSeconds) {
        if (!cache || !cache.windField) {
            return;
        }

        const weatherVisualResponse = getWeatherVisualResponse();
        const windField = cache.windField;
        if (!state || state.appState !== "SITE_ACTIVE" || weatherVisualResponse.windLevel <= visibleWindStreakThreshold) {
            windField.group.visible = false;
            return;
        }

        const windLevel = weatherVisualResponse.windLevel;
        const windDensityLevel = Math.pow(clamp01(windLevel), 1.45);
        const windSpeedLevel = Math.pow(clamp01(windLevel), 1.2);
        const directionDegrees = resolveVisualWindDirectionDegrees();
        const directionRadians = directionDegrees * Math.PI / 180.0;
        const alpha = Math.min(0.2, 0.015 + windDensityLevel * 0.185);
        const streakLength = lerp(0.72, 2.1, windDensityLevel);
        const streakWidth = lerp(0.62, 1.08, windDensityLevel);
        const flowDistance = windField.depthSpan * (0.94 + windSpeedLevel * 1.28);
        const targetActiveStreakCount = Math.max(
            2,
            Math.min(
                windField.streaks.length,
                Math.round(lerp(3, windField.streaks.length * 0.72, windDensityLevel))
            )
        );
        const minRespawnDelaySeconds = lerp(0.52, 0.05, windLevel);
        const maxRespawnDelaySeconds = lerp(1.35, 0.16, windLevel);

        windField.group.visible = alpha > 0.02;
        windField.group.rotation.y = directionRadians;

        let activeStreakCount = 0;
        windField.streaks.forEach((streak) => {
            if (streak.active && elapsedSeconds >= streak.spawnTime + streak.lifeSeconds) {
                scheduleWindStreakRespawn(streak, elapsedSeconds, minRespawnDelaySeconds, maxRespawnDelaySeconds);
            }
            if (streak.active) {
                activeStreakCount += 1;
            }
        });

        windField.streaks.forEach((streak) => {
            if (!streak.active &&
                elapsedSeconds >= streak.nextSpawnAt &&
                activeStreakCount < targetActiveStreakCount) {
                activateWindStreak(streak, windField, elapsedSeconds, windDensityLevel, windSpeedLevel, flowDistance);
                activeStreakCount += 1;
            }
        });

        const minimumContinuousStreakCount = Math.min(targetActiveStreakCount, 2);
        if (activeStreakCount < minimumContinuousStreakCount) {
            windField.streaks.forEach((streak) => {
                if (!streak.active && activeStreakCount < minimumContinuousStreakCount) {
                    activateWindStreak(streak, windField, elapsedSeconds, windDensityLevel, windSpeedLevel, flowDistance);
                    activeStreakCount += 1;
                }
            });
        }

        windField.streaks.forEach((streak, index) => {
            if (!streak.active) {
                streak.mesh.material.opacity = 0.0;
                return;
            }

            const progress = clamp01((elapsedSeconds - streak.spawnTime) / Math.max(streak.lifeSeconds, 0.0001));
            const flowOffset = lerp(-0.5 * streak.travelSpan, 0.5 * streak.travelSpan, progress);
            const lateralOffset =
                lerp(streak.streamStartX, streak.streamEndX, progress) +
                Math.sin(
                    elapsedSeconds * (0.18 + streak.speedScale * 0.14) +
                    streak.lateralDriftPhase * Math.PI * 2.0) *
                    windField.horizontalSpan *
                    (0.01 + streak.lateralDriftSeed * 0.022);
            const verticalBob =
                Math.sin((elapsedSeconds + streak.swaySeed * 6.0) * (0.84 + windSpeedLevel * 1.25)) *
                (0.014 + windDensityLevel * 0.018);
            const fadeIn = smoothstep01(0.0, 0.14, progress);
            const fadeOut = 1.0 - smoothstep01(0.72, 1.0, progress);
            streak.mesh.position.x = lateralOffset;
            streak.mesh.position.z = flowOffset;
            streak.mesh.position.y = streak.streamY + streak.verticalSeed + verticalBob;
            streak.mesh.rotation.z =
                streak.rotationJitter +
                Math.sin(elapsedSeconds * (0.55 + windSpeedLevel * 0.65) + index * 0.4) * 0.035;
            streak.mesh.scale.set(
                streakWidth * streak.widthScale * streak.streamWidthScale,
                streakLength * streak.lengthScale * streak.streamLengthScale,
                1.0
            );
            streak.mesh.material.opacity = alpha * streak.opacityScale * fadeIn * fadeOut;
            streak.mesh.material.color.setHex(0xf6edd7);
        });
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
            (currentActionKind === 1 || currentActionKind === 2 || currentActionKind === 6 || currentActionKind === 9)
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
                    Math.round((tile.sandBurial || 0) * 100),
                    Math.round((tile.moisture || 0) * 100),
                    Math.round((tile.soilFertility || 0) * 100),
                    Math.round((tile.soilSalinity || 0) * 100),
                    tile.visibleExcavationDepth || 0
                ])
            }
        );
    }

    function setPlantVisualWindStrength(plantVisual, localWind) {
        const plantWindStrength = clamp01((localWind || 0) / 80.0);
        plantVisual.traverse((node) => {
            if (node && node.isMesh) {
                node.userData.plantWindStrength = plantWindStrength;
            }
        });
    }

    function buildPlantVisualRenderSpec(tileMap, plantVisual) {
        if (!tileMap || !plantVisual || !plantVisual.userData) {
            return null;
        }

        const anchorX = plantVisual.userData.plantAnchorX;
        const anchorY = plantVisual.userData.plantAnchorY;
        const footprintWidth = Math.max(1, plantVisual.userData.plantFootprintWidth || 1);
        const footprintHeight = Math.max(1, plantVisual.userData.plantFootprintHeight || 1);
        if (typeof anchorX !== "number" || typeof anchorY !== "number") {
            return null;
        }

        const anchorTile = tileMap.get(buildSiteTileKey(anchorX, anchorY));
        if (!anchorTile) {
            return null;
        }

        const footprintTiles = [];
        let footprintComplete = true;
        for (let offsetY = 0; offsetY < footprintHeight; offsetY += 1) {
            for (let offsetX = 0; offsetX < footprintWidth; offsetX += 1) {
                const footprintTile = tileMap.get(buildSiteTileKey(anchorX + offsetX, anchorY + offsetY));
                if (!footprintTile || footprintTile.plantTypeId !== anchorTile.plantTypeId) {
                    footprintComplete = false;
                    break;
                }
                footprintTiles.push(footprintTile);
            }
            if (!footprintComplete) {
                break;
            }
        }

        return buildPlantRenderSpec(
            anchorTile,
            footprintComplete ? footprintTiles : [anchorTile],
            footprintComplete
                ? { width: footprintWidth, height: footprintHeight }
                : { width: 1, height: 1 }
        );
    }

    function computePlantVisualVerticalScale(plantSpec) {
        if (!plantSpec || !plantSpec.anchorTile) {
            return 1.0;
        }

        const plantDensity = clamp01(plantSpec.plantDensity || 0);
        if ((plantSpec.anchorTile.plantTypeId || 0) === 5) {
            const plantDensityScale = smoothstep01(0.0, 1.0, plantDensity);
            return 0.55 + plantDensityScale * 1.55 + (plantSpec.areaScale - 1.0) * 0.08;
        }

        return 0.88 + plantDensity * 0.34 + (plantSpec.areaScale - 1.0) * 0.08;
    }

    function updateSitePlantVisualEcology(cache, siteBootstrap) {
        if (!cache || !cache.plantVisuals || !siteBootstrap || !siteBootstrap.tiles) {
            return;
        }

        const tileMap = buildSiteTileMap(siteBootstrap.tiles);
        cache.plantVisuals.forEach((plantVisual) => {
            const plantSpec = buildPlantVisualRenderSpec(tileMap, plantVisual);
            if (!plantSpec) {
                return;
            }

            plantVisual.scale.y = computePlantVisualVerticalScale(plantSpec);
            setPlantVisualWindStrength(plantVisual, plantSpec.localWind);
        });
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

    function applySiteCameraBootstrapTransform(siteBootstrap, siteState, width, height, offsetX, offsetZ, previousCache) {
        const cameraOrbitState = buildSiteCameraOrbitState(siteBootstrap, width, height, previousCache);
        const worker = siteState && siteState.worker ? siteState.worker : null;
        const focusX = worker ? worker.tileX - offsetX : 0.0;
        const focusZ = worker ? worker.tileY - offsetZ : 0.0;
        camera.position.set(
            focusX + cameraOrbitState.offsetX,
            cameraOrbitState.offsetY,
            focusZ + cameraOrbitState.offsetZ);
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

    function resolvePlantFootprintByType(plantTypeId) {
        return plantFootprintCatalog[plantTypeId] || { width: 1, height: 1 };
    }

    function alignTileAxisToSpan(coord, span) {
        const normalizedSpan = Math.max(1, span || 1);
        const remainder = coord % normalizedSpan;
        if (remainder === 0) {
            return coord;
        }
        return coord - (remainder < 0 ? (remainder + normalizedSpan) : remainder);
    }

    function buildSiteTileMap(tiles) {
        const tileMap = new Map();
        (tiles || []).forEach((tile) => {
            tileMap.set(buildSiteTileKey(tile.x, tile.y), tile);
        });
        return tileMap;
    }

    function buildSiteTileKey(tileX, tileY) {
        return tileX + ":" + tileY;
    }

    function cloneActiveWitheringAlerts(previousCache, currentTimeSeconds) {
        if (!previousCache || !previousCache.witherAlerts) {
            return [];
        }

        const carriedAlerts = [];
        previousCache.witherAlerts.forEach((alertState, alertKey) => {
            if (!alertState || !(alertState.expiresAtSeconds > currentTimeSeconds)) {
                return;
            }

            carriedAlerts.push({
                key: alertKey,
                tileX: alertState.tileX,
                tileY: alertState.tileY,
                startTimeSeconds: alertState.startTimeSeconds,
                durationSeconds: alertState.durationSeconds,
                style: alertState.style || densityDecreaseAlertStyle
            });
        });
        return carriedAlerts;
    }

    function resolveDensityAlertStyle(densityDelta) {
        return densityDelta >= 0
            ? densityIncreaseAlertStyle
            : densityDecreaseAlertStyle;
    }

    function collectWitheringAlertTriggers(previousBootstrap, nextBootstrap) {
        if (!previousBootstrap ||
            !nextBootstrap ||
            previousBootstrap.siteId !== nextBootstrap.siteId) {
            return [];
        }

        const previousTileMap = buildSiteTileMap(previousBootstrap.tiles);
        const triggeredAlerts = [];
        (nextBootstrap.tiles || []).forEach((tile) => {
            const previousTile = previousTileMap.get(buildSiteTileKey(tile.x, tile.y));
            if (!previousTile) {
                return;
            }

            const previousDensity =
                typeof previousTile.plantDensity === "number" ? previousTile.plantDensity : 0;
            const nextDensity =
                typeof tile.plantDensity === "number" ? tile.plantDensity : 0;
            const densityDelta = nextDensity - previousDensity;
            if (Math.abs(densityDelta) <= witheringAlertDensityDropThreshold) {
                return;
            }

            const previousPlantTypeId = previousTile.plantTypeId || 0;
            const nextPlantTypeId = tile.plantTypeId || 0;
            if (constantWitheringPlantTypeIds.has(previousPlantTypeId) ||
                constantWitheringPlantTypeIds.has(nextPlantTypeId)) {
                return;
            }
            const hadPlant = previousPlantTypeId !== 0 || previousDensity > witheringAlertDensityDropThreshold;
            const hasPlant = nextPlantTypeId !== 0 || nextDensity > witheringAlertDensityDropThreshold;
            if (!hadPlant && !hasPlant) {
                return;
            }

            triggeredAlerts.push({
                tileX: tile.x,
                tileY: tile.y,
                style: resolveDensityAlertStyle(densityDelta)
            });
        });
        return triggeredAlerts;
    }

    function createWitheringAlertMesh(style) {
        const resolvedStyle = style || densityDecreaseAlertStyle;
        const material = new THREE_NS.MeshStandardMaterial({
            color: resolvedStyle.colorHex,
            emissive: resolvedStyle.emissiveHex,
            emissiveIntensity: 0.0,
            roughness: 0.34,
            metalness: 0.0,
            transparent: true,
            opacity: 0.0,
            depthWrite: false,
            side: THREE_NS.DoubleSide,
            blending: THREE_NS.AdditiveBlending,
            toneMapped: false
        });
        const mesh = new THREE_NS.Mesh(
            new THREE_NS.PlaneGeometry(0.82, 0.82),
            material
        );
        mesh.rotation.x = -Math.PI / 2;
        mesh.renderOrder = 8;
        return mesh;
    }

    function removeWitheringAlert(cache, alertKey) {
        if (!cache || !cache.witherAlerts || !cache.witherAlerts.has(alertKey)) {
            return;
        }

        const alertState = cache.witherAlerts.get(alertKey);
        if (alertState && alertState.mesh) {
            if (alertState.mesh.parent) {
                alertState.mesh.parent.remove(alertState.mesh);
            }
            disposeObject(alertState.mesh);
        }
        cache.witherAlerts.delete(alertKey);
    }

    function resolveRenderedTileHeight(cache, tile) {
        if (cache && cache.tilePickables) {
            for (let index = 0; index < cache.tilePickables.length; index += 1) {
                const pickMesh = cache.tilePickables[index];
                if (!pickMesh || !pickMesh.userData) {
                    continue;
                }
                if (pickMesh.userData.tileX === tile.x && pickMesh.userData.tileY === tile.y) {
                    return (pickMesh.position && typeof pickMesh.position.y === "number")
                        ? Math.max(0, pickMesh.position.y - 0.025)
                        : computeSiteTileVisualHeight(tile);
                }
            }
        }

        return computeSiteTileVisualHeight(tile);
    }

    function attachWitheringAlert(cache, tileX, tileY, startTimeSeconds, durationSeconds, style) {
        if (!cache || !cache.witherAlertGroup || !cache.sourceBootstrap) {
            return;
        }

        const tileMap = buildSiteTileMap(cache.sourceBootstrap.tiles);
        const tile = tileMap.get(buildSiteTileKey(tileX, tileY));
        if (!tile) {
            return;
        }

        const alertKey = buildSiteTileKey(tileX, tileY);
        const existingAlertState =
            cache.witherAlerts && cache.witherAlerts.has(alertKey)
                ? cache.witherAlerts.get(alertKey)
                : null;
        if (existingAlertState && existingAlertState.expiresAtSeconds > startTimeSeconds) {
            return;
        }
        removeWitheringAlert(cache, alertKey);

        const resolvedStyle = style || densityDecreaseAlertStyle;
        const mesh = createWitheringAlertMesh(resolvedStyle);
        const tileHeight = resolveRenderedTileHeight(cache, tile);
        mesh.position.set(
            tileX - cache.offsetX,
            tileHeight + witheringAlertHeightOffset,
            tileY - cache.offsetZ
        );
        cache.witherAlertGroup.add(mesh);
        cache.witherAlerts.set(alertKey, {
            tileX: tileX,
            tileY: tileY,
            mesh: mesh,
            startTimeSeconds: startTimeSeconds,
            durationSeconds: durationSeconds,
            expiresAtSeconds: startTimeSeconds + durationSeconds,
            style: resolvedStyle
        });
    }

    function restoreWitheringAlerts(cache, carriedAlerts) {
        (carriedAlerts || []).forEach((alertState) => {
            attachWitheringAlert(
                cache,
                alertState.tileX,
                alertState.tileY,
                alertState.startTimeSeconds,
                alertState.durationSeconds || witheringAlertDurationSeconds,
                alertState.style
            );
        });
    }

    function triggerWitheringAlerts(cache, triggeredAlerts, startTimeSeconds) {
        (triggeredAlerts || []).forEach((alertState) => {
            attachWitheringAlert(
                cache,
                alertState.tileX,
                alertState.tileY,
                startTimeSeconds,
                witheringAlertDurationSeconds,
                alertState.style
            );
        });
    }

    function updateWitheringAlerts(cache, currentTimeSeconds) {
        if (!cache || !cache.witherAlerts || cache.witherAlerts.size === 0) {
            return;
        }

        const expiredAlerts = [];
        cache.witherAlerts.forEach((alertState, alertKey) => {
            if (!alertState || !alertState.mesh || !alertState.mesh.material) {
                expiredAlerts.push(alertKey);
                return;
            }

            const durationSeconds = Math.max(alertState.durationSeconds || 0, 0.0001);
            const normalizedTime = clamp01((currentTimeSeconds - alertState.startTimeSeconds) / durationSeconds);
            if (normalizedTime >= 1.0) {
                expiredAlerts.push(alertKey);
                return;
            }

            const opacity = Math.sin(normalizedTime * Math.PI) * witheringAlertMaxOpacity;
            const style = alertState.style || densityDecreaseAlertStyle;
            alertState.mesh.material.opacity = opacity;
            alertState.mesh.material.emissiveIntensity =
                style.emissiveBaseIntensity + opacity * style.emissiveBoostIntensity;
        });

        expiredAlerts.forEach((alertKey) => {
            removeWitheringAlert(cache, alertKey);
        });
    }

    function buildPlantRenderSpec(anchorTile, footprintTiles, footprint) {
        const tiles = footprintTiles && footprintTiles.length > 0 ? footprintTiles : [anchorTile];
        const footprintWidth = Math.max(1, footprint.width || 1);
        const footprintHeight = Math.max(1, footprint.height || 1);
        const footprintArea = Math.max(1, footprintWidth * footprintHeight);
        const areaScale = Math.sqrt(footprintArea);
        const plantDensity =
            tiles.reduce((total, tile) => total + clamp01(tile.plantDensity || 0), 0) /
            Math.max(tiles.length, 1);
        const localWind =
            tiles.reduce((total, tile) => total + Math.max(0, tile.localWind || 0), 0) /
            Math.max(tiles.length, 1);
        const baseHeight =
            tiles.reduce((total, tile) => total + computeSiteTileVisualHeight(tile), 0) /
            Math.max(tiles.length, 1);
        const fullSpanX = 0.74 + Math.max(0, footprintWidth - 1) * 0.92;
        const fullSpanZ = 0.74 + Math.max(0, footprintHeight - 1) * 0.92;

        return {
            anchorTile: anchorTile,
            footprintTiles: tiles,
            plantTypeId: anchorTile.plantTypeId || 0,
            plantDensity: plantDensity,
            localWind: localWind,
            footprintWidth: footprintWidth,
            footprintHeight: footprintHeight,
            footprintArea: footprintArea,
            areaScale: areaScale,
            fullSpanX: fullSpanX,
            fullSpanZ: fullSpanZ,
            halfSpanX: fullSpanX * 0.5,
            halfSpanZ: fullSpanZ * 0.5,
            baseHeight: baseHeight,
            centerX: anchorTile.x + (footprintWidth - 1) * 0.5,
            centerZ: anchorTile.y + (footprintHeight - 1) * 0.5
        };
    }

    function collectPlantVisualSpecs(tiles) {
        const tileMap = buildSiteTileMap(tiles);
        const specs = [];

        (tiles || []).forEach((tile) => {
            const plantDensity = clamp01(tile && tile.plantDensity ? tile.plantDensity : 0);
            const hasPlant = tile && (tile.plantTypeId !== 0 || plantDensity > 0.05);
            if (!hasPlant) {
                return;
            }

            const footprint = resolvePlantFootprintByType(tile.plantTypeId || 0);
            const anchorX = alignTileAxisToSpan(tile.x, footprint.width);
            const anchorY = alignTileAxisToSpan(tile.y, footprint.height);
            if (tile.x !== anchorX || tile.y !== anchorY) {
                return;
            }

            const footprintTiles = [];
            let footprintComplete = true;
            for (let offsetY = 0; offsetY < footprint.height; offsetY += 1) {
                for (let offsetX = 0; offsetX < footprint.width; offsetX += 1) {
                    const footprintTile = tileMap.get((anchorX + offsetX) + ":" + (anchorY + offsetY));
                    if (!footprintTile || footprintTile.plantTypeId !== tile.plantTypeId) {
                        footprintComplete = false;
                        break;
                    }
                    footprintTiles.push(footprintTile);
                }
                if (!footprintComplete) {
                    break;
                }
            }

            specs.push(
                buildPlantRenderSpec(
                    tile,
                    footprintComplete ? footprintTiles : [tile],
                    footprintComplete ? footprint : { width: 1, height: 1 }
                )
            );
        });

        return specs;
    }

    function applyPlantWindShader(material, windFlexibility) {
        const flexibility = windFlexibility == null ? 1.0 : windFlexibility;
        material.onBeforeCompile = function (shader) {
            shader.uniforms.uPlantTime = plantWindShaderUniforms.uPlantTime;
            shader.uniforms.uPlantWindStrength = plantWindShaderUniforms.uPlantWindStrength;
            shader.uniforms.uPlantWindDirection = plantWindShaderUniforms.uPlantWindDirection;
            shader.uniforms.uPlantLocalWindStrength = { value: 0.0 };
            shader.uniforms.uPlantWindFlexibility = { value: flexibility };
            material.userData.plantWindShader = shader;
            shader.vertexShader = shader.vertexShader.replace(
                "#include <common>",
                "#include <common>\n" +
                "uniform float uPlantTime;\n" +
                "uniform float uPlantWindStrength;\n" +
                "uniform vec2 uPlantWindDirection;\n" +
                "uniform float uPlantLocalWindStrength;\n" +
                "uniform float uPlantWindFlexibility;\n"
            );
            shader.vertexShader = shader.vertexShader.replace(
                "#include <begin_vertex>",
                "#include <begin_vertex>\n" +
                "float siteWindLevel = clamp(uPlantWindStrength, 0.0, 1.0);\n" +
                "float plantWindLevel = clamp(uPlantLocalWindStrength, 0.0, 1.0);\n" +
                "float visiblePlantWindLevel = max(plantWindLevel, siteWindLevel * 0.22);\n" +
                "float synchronizedGustLevel = max(visiblePlantWindLevel, siteWindLevel * 0.35);\n" +
                "vec4 plantWorldPosition = modelMatrix * vec4(transformed, 1.0);\n" +
                "vec3 modelBasisX = normalize(vec3(modelMatrix[0].x, modelMatrix[0].y, modelMatrix[0].z));\n" +
                "vec3 modelBasisZ = normalize(vec3(modelMatrix[2].x, modelMatrix[2].y, modelMatrix[2].z));\n" +
                "vec3 worldWindDirection = normalize(vec3(uPlantWindDirection.x, 0.0, uPlantWindDirection.y));\n" +
                "vec3 localWindDirection = vec3(dot(worldWindDirection, modelBasisX), 0.0, dot(worldWindDirection, modelBasisZ));\n" +
                "localWindDirection.y = 0.0;\n" +
                "float localWindLength = max(length(localWindDirection), 0.0001);\n" +
                "localWindDirection /= localWindLength;\n" +
                "float plantVerticalWeight = smoothstep(-0.22, 0.38, transformed.y);\n" +
                "float plantWave = sin(dot(plantWorldPosition.xz, vec2(0.38, 0.54)) + uPlantTime * (1.6 + synchronizedGustLevel * 3.4));\n" +
                "float plantGust = sin(dot(plantWorldPosition.xz, vec2(-0.84, 1.18)) * 1.7 + uPlantTime * (2.8 + synchronizedGustLevel * 4.1));\n" +
                "float plantFlutter = sin(uPlantTime * (4.4 + synchronizedGustLevel * 2.1) + plantWorldPosition.y * 1.8 + plantWorldPosition.x * 0.6);\n" +
                "float plantSway = (plantWave * 0.58 + plantGust * 0.28 + plantFlutter * 0.14) * plantVerticalWeight * visiblePlantWindLevel * uPlantWindFlexibility;\n" +
                "transformed += localWindDirection * plantSway * (0.025 + visiblePlantWindLevel * 0.11);\n" +
                "transformed.y += abs(plantGust) * plantVerticalWeight * visiblePlantWindLevel * uPlantWindFlexibility * 0.028;\n"
            );
        };
        material.onBeforeRender = function (_renderer, _scene, _camera, _geometry, object) {
            const shader = material.userData.plantWindShader;
            if (!shader || !shader.uniforms || !shader.uniforms.uPlantLocalWindStrength) {
                return;
            }

            const perMeshWindStrength =
                object && object.userData && typeof object.userData.plantWindStrength === "number"
                    ? object.userData.plantWindStrength
                    : 0.0;
            shader.uniforms.uPlantLocalWindStrength.value = clamp01(perMeshWindStrength);
        };
        material.customProgramCacheKey = function () {
            return "plant-wind-v3";
        };
        return material;
    }

    function updatePlantWindShaderUniforms(state, elapsedSeconds) {
        if (!state) {
            plantWindShaderUniforms.uPlantTime.value = elapsedSeconds;
            plantWindShaderUniforms.uPlantWindStrength.value = 0.0;
            plantWindShaderUniforms.uPlantWindDirection.value.set(0.0, 1.0);
            return;
        }

        const weatherVisualResponse = getWeatherVisualResponse();
        const windStrength = clamp01(weatherVisualResponse.windLevel || 0);
        const directionDegrees = resolveVisualWindDirectionDegrees();
        const directionRadians = directionDegrees * Math.PI / 180.0;

        plantWindShaderUniforms.uPlantTime.value = elapsedSeconds;
        plantWindShaderUniforms.uPlantWindStrength.value = windStrength;
        plantWindShaderUniforms.uPlantWindDirection.value.set(
            Math.sin(directionRadians),
            Math.cos(directionRadians)
        );
    }

    function createPlantMaterial(color, roughness, emissive, emissiveIntensity, options) {
        const windReactive = !options || options.windReactive !== false;
        const windFlexibility =
            options && typeof options.windFlexibility === "number"
                ? options.windFlexibility
                : 1.0;
        const material = tagMergeableMaterial(
            new THREE_NS.MeshStandardMaterial({
                color: color,
                roughness: roughness == null ? 0.84 : roughness,
                metalness: 0.01,
                emissive: emissive == null ? 0x000000 : emissive,
                emissiveIntensity: emissiveIntensity == null ? 0.0 : emissiveIntensity
            }),
            {
                windReactive: windReactive,
                windFlexibility: windReactive ? windFlexibility : null
            }
        );
        if (windReactive) {
            applyPlantWindShader(material, windFlexibility);
        }
        return material;
    }

    function createLeafMesh(color, width, thickness, length, windFlexibility) {
        const leaf = new THREE_NS.Mesh(
            new THREE_NS.SphereGeometry(1, 10, 10),
            createPlantMaterial(color, 0.78, color, 0.04, {
                windFlexibility: windFlexibility == null ? 1.0 : windFlexibility
            })
        );
        leaf.scale.set(width, thickness, length);
        return leaf;
    }

    function createGrassBladeMesh(color, width, height, windFlexibility) {
        const geometry = new THREE_NS.PlaneGeometry(width, height, 1, 5);
        geometry.translate(0, height * 0.5, 0);
        const blade = new THREE_NS.Mesh(
            geometry,
            createPlantMaterial(color, 0.74, color, 0.05, {
                windFlexibility: windFlexibility == null ? 1.0 : windFlexibility
            })
        );
        blade.material.side = THREE_NS.DoubleSide;
        return blade;
    }

    function createBerryMesh(color, radius, windFlexibility) {
        return new THREE_NS.Mesh(
            new THREE_NS.SphereGeometry(radius, 8, 8),
            createPlantMaterial(color, 0.56, color, 0.08, {
                windFlexibility: windFlexibility == null ? 0.48 : windFlexibility
            })
        );
    }

    function createBranchMesh(color, baseRadius, tipRadius, length, sides, windFlexibility) {
        const branch = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(
                tipRadius,
                baseRadius,
                length,
                sides == null ? 5 : sides
            ),
            createPlantMaterial(color, 0.88, 0x23180f, 0.0, {
                windFlexibility: windFlexibility == null ? 0.32 : windFlexibility
            })
        );
        return branch;
    }

    function resolvePlantPalette(tile) {
        if (tile.plantTypeId === 1) {
            return { primary: 0x8fb76b, secondary: 0x6f8d49, glow: 0x4f652e };
        }
        if (tile.plantTypeId === 2) {
            return { primary: 0x8ba762, secondary: 0x6a7e44, glow: 0x45542b };
        }
        if (tile.plantTypeId === 3) {
            return { primary: 0x8aa160, secondary: 0x64794a, glow: 0x405029 };
        }
        if (tile.plantTypeId === 4) {
            return { primary: 0x9abd66, secondary: 0x728c42, glow: 0x4b5a29 };
        }
        if (tile.plantTypeId === 5) {
            return { primary: 0xcaa15c, secondary: 0xa67b3e, glow: 0x6d4f21 };
        }
        if (tile.plantTypeId === 6) {
            return { primary: 0x91ad5e, secondary: 0x6d8642, glow: 0x45552a };
        }
        if (tile.plantTypeId === 7) {
            return { primary: 0x8dbb74, secondary: 0x648c56, glow: 0x41613c };
        }
        if (tile.plantTypeId === 8) {
            return { primary: 0x9bb37a, secondary: 0x738656, glow: 0x4c5839 };
        }
        if (tile.plantTypeId === 9) {
            return { primary: 0x6f9457, secondary: 0x4d6a3d, glow: 0x314227 };
        }
        if (tile.plantTypeId === 10) {
            return { primary: 0x89a36c, secondary: 0x61764f, glow: 0x43513a };
        }
        return { primary: 0x95b866, secondary: 0x728f4b, glow: 0x4b5f31 };
    }

    function addStrawCheckerboardPlant(plantGroup, plantSpec, palette) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const densityHeightScale = smoothstep01(0.0, 1.0, plantDensity);
        const fullSpanX = plantSpec.fullSpanX;
        const fullSpanZ = plantSpec.fullSpanZ;
        const halfSpanX = plantSpec.halfSpanX;
        const halfSpanZ = plantSpec.halfSpanZ;
        const linePadding = 0.05;
        const bundleThickness = 0.022 + plantDensity * 0.014;
        const baseBladeHeight = 0.15 + densityHeightScale * 0.14 + (plantSpec.areaScale - 1.0) * 0.02;
        const baseBladeWidth = 0.04 + plantDensity * 0.018;
        const baseLift = 0.01 + densityHeightScale * 0.018;
        const lineCountX = Math.max(8, Math.round(fullSpanX * (8 + plantDensity * 4)));
        const lineCountZ = Math.max(8, Math.round(fullSpanZ * (8 + plantDensity * 4)));
        const strawColors = [palette.primary, palette.secondary, 0xd9bb74];

        function addBarrierBundle(x, z, orientation, seedBase) {
            const bladeCount =
                3 +
                (deterministicNoise01(tile.x, tile.y, seedBase + 1) > 0.56 ? 1 : 0);
            const alongX = Math.sin(orientation);
            const alongZ = Math.cos(orientation);
            const perpX = alongZ;
            const perpZ = -alongX;
            const bundleLift = baseLift + deterministicNoise01(tile.x, tile.y, seedBase + 5) * 0.014;

            for (let bladeIndex = 0; bladeIndex < bladeCount; bladeIndex += 1) {
                const bladeSeed = seedBase + bladeIndex * 53;
                const sideOffset =
                    (deterministicNoise01(tile.y, tile.x, bladeSeed + 7) - 0.5) * bundleThickness;
                const alongOffset =
                    (deterministicNoise01(tile.x, tile.y, bladeSeed + 11) - 0.5) * 0.02;
                const blade = createGrassBladeMesh(
                    strawColors[
                        Math.floor(deterministicNoise01(tile.x, tile.y, bladeSeed + 13) * strawColors.length) % strawColors.length
                    ],
                    baseBladeWidth * (0.85 + deterministicNoise01(tile.y, tile.x, bladeSeed + 17) * 0.45),
                    baseBladeHeight * (0.72 + deterministicNoise01(tile.x, tile.y, bladeSeed + 19) * 0.58),
                    0.82
                );
                blade.position.set(
                    x + perpX * sideOffset + alongX * alongOffset,
                    bundleLift + deterministicNoise01(tile.x, tile.y, bladeSeed + 23) * 0.012,
                    z + perpZ * sideOffset + alongZ * alongOffset
                );
                blade.rotation.y =
                    orientation +
                    (bladeIndex % 2 === 0 ? 0 : Math.PI * 0.5) +
                    (deterministicNoise01(tile.y, tile.x, bladeSeed + 29) - 0.5) * 0.16;
                blade.rotation.z = 0.08 + deterministicNoise01(tile.x, tile.y, bladeSeed + 31) * 0.22;
                blade.rotation.x = -0.05 + deterministicNoise01(tile.y, tile.x, bladeSeed + 37) * 0.1;
                plantGroup.add(blade);
            }
        }

        function addBarrierLine(startX, startZ, endX, endZ, count, seedBase) {
            const orientation = Math.atan2(endX - startX, endZ - startZ);
            for (let index = 0; index < count; index += 1) {
                const t = count <= 1 ? 0.5 : index / (count - 1);
                const x = THREE_NS.MathUtils.lerp(startX, endX, t);
                const z = THREE_NS.MathUtils.lerp(startZ, endZ, t);
                addBarrierBundle(x, z, orientation, seedBase + index * 47);
            }
        }

        addBarrierLine(
            -halfSpanX + linePadding,
            -halfSpanZ + linePadding,
            halfSpanX - linePadding,
            -halfSpanZ + linePadding,
            lineCountX,
            101
        );
        addBarrierLine(
            -halfSpanX + linePadding,
            halfSpanZ - linePadding,
            halfSpanX - linePadding,
            halfSpanZ - linePadding,
            lineCountX,
            401
        );
        addBarrierLine(
            -halfSpanX + linePadding,
            -halfSpanZ + linePadding,
            -halfSpanX + linePadding,
            halfSpanZ - linePadding,
            lineCountZ,
            701
        );
        addBarrierLine(
            halfSpanX - linePadding,
            -halfSpanZ + linePadding,
            halfSpanX - linePadding,
            halfSpanZ - linePadding,
            lineCountZ,
            1001
        );
        const edgePosts = [
            [-halfSpanX + linePadding, -halfSpanZ + linePadding],
            [halfSpanX - linePadding, -halfSpanZ + linePadding],
            [-halfSpanX + linePadding, halfSpanZ - linePadding],
            [halfSpanX - linePadding, halfSpanZ - linePadding]
        ];
        edgePosts.push([0, -halfSpanZ + linePadding]);
        edgePosts.push([0, halfSpanZ - linePadding]);
        edgePosts.push([-halfSpanX + linePadding, 0]);
        edgePosts.push([halfSpanX - linePadding, 0]);
        edgePosts.forEach((postPosition, postIndex) => {
            const postSeed = 4001 + postIndex * 59;
            const postHeight = 0.06 + densityHeightScale * 0.06 + deterministicNoise01(tile.y, tile.x, postSeed + 11) * 0.02;
            const post = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.02, postHeight, 0.02),
                createPlantMaterial(palette.secondary, 0.92, 0x4b3513, 0.0, {
                    windReactive: false
                })
            );
            post.position.set(postPosition[0], postHeight * 0.5, postPosition[1]);
            post.rotation.y = deterministicNoise01(tile.x, tile.y, postSeed + 13) * Math.PI * 0.2;
            plantGroup.add(post);
        });
    }

    function addPlantDensityShell(plantGroup, plantSpec, palette) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const shellCount =
            Math.max(4, Math.round((4 + plantDensity * 10) * Math.max(1.0, plantSpec.areaScale * 0.72)));
        const shellRadiusX = plantSpec.halfSpanX * (0.48 + plantDensity * 0.28);
        const shellRadiusZ = plantSpec.halfSpanZ * (0.48 + plantDensity * 0.28);
        const shellHeight = 0.03 + plantDensity * 0.11;

        for (let index = 0; index < shellCount; index += 1) {
            const angle = deterministicNoise01(tile.x, tile.y, index + 171) * Math.PI * 2;
            const radius = 0.18 + deterministicNoise01(tile.y, tile.x, index + 181) * 0.82;
            const frond = createLeafMesh(
                index % 3 === 0 ? palette.secondary : palette.primary,
                0.055 + plantDensity * 0.06,
                0.012 + plantDensity * 0.01,
                0.09 + plantDensity * 0.12,
                1.1
            );
            frond.position.set(
                Math.cos(angle) * shellRadiusX * radius,
                shellHeight + deterministicNoise01(tile.x, tile.y, index + 191) * 0.05,
                Math.sin(angle) * shellRadiusZ * radius
            );
            frond.rotation.y = angle;
            frond.rotation.x = -0.18 + deterministicNoise01(tile.y, tile.x, index + 201) * 0.36;
            frond.rotation.z = deterministicNoise01(tile.x, tile.y, index + 211) * Math.PI;
            plantGroup.add(frond);
        }

        if (plantDensity >= 0.45) {
            const canopy = new THREE_NS.Mesh(
                new THREE_NS.SphereGeometry(
                    0.18 + plantDensity * 0.12 + (plantSpec.areaScale - 1.0) * 0.06,
                    14,
                    14
                ),
                createPlantMaterial(
                    palette.primary,
                    0.86,
                    palette.glow,
                    0.03 + plantDensity * 0.04,
                    { windFlexibility: 0.92 }
                )
            );
            canopy.position.y = 0.06 + plantDensity * 0.08;
            canopy.scale.set(
                1.0 + plantDensity * 0.18 + (plantSpec.footprintWidth - 1) * 0.9,
                0.22 + plantDensity * 0.22,
                1.0 + plantDensity * 0.18 + (plantSpec.footprintHeight - 1) * 0.9
            );
            plantGroup.add(canopy);
        }
    }

    function addOrdosWormwoodPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const stemColor = 0x7f8560;
        const leafColors = [0xb5c69a, 0xa8bc8c, 0x96ab79];
        const bunchCount =
            Math.max(12, Math.round((12 + plantDensity * 12) * Math.max(1.0, plantSpec.areaScale * 0.9)));
        const moundHeight = 0.18 + plantDensity * 0.14 + (plantSpec.areaScale - 1.0) * 0.05;

        for (let index = 0; index < bunchCount; index += 1) {
            const angle =
                (index / bunchCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 1) * 0.42;
            const offsetRadius = 0.16 + deterministicNoise01(tile.y, tile.x, index + 11) * 0.72;
            const stemHeight = moundHeight * (0.85 + deterministicNoise01(tile.x, tile.y, index + 21) * 1.15);
            const stemRadius =
                (0.012 + deterministicNoise01(tile.y, tile.x, index + 31) * 0.008) *
                (1.0 + (plantSpec.areaScale - 1.0) * 0.08);
            const stem = createBranchMesh(stemColor, stemRadius, stemRadius * 0.52, stemHeight, 5, 0.72);
            stem.position.set(
                Math.cos(angle) * plantSpec.halfSpanX * offsetRadius,
                stemHeight * 0.5,
                Math.sin(angle) * plantSpec.halfSpanZ * offsetRadius
            );
            stem.rotation.z = Math.cos(angle) * 0.12;
            stem.rotation.x = -Math.sin(angle) * 0.1;
            plantGroup.add(stem);

            const sprigCount = 2 + (index % 3);
            for (let sprigIndex = 0; sprigIndex < sprigCount; sprigIndex += 1) {
                const frond = createLeafMesh(
                    leafColors[(index + sprigIndex) % leafColors.length],
                    0.018 + deterministicNoise01(tile.x, tile.y, index + sprigIndex + 41) * 0.01,
                    0.008,
                    0.08 + plantDensity * 0.05 + deterministicNoise01(tile.y, tile.x, index + sprigIndex + 51) * 0.05,
                    1.32
                );
                frond.position.set(
                    stem.position.x * (0.72 + sprigIndex * 0.1),
                    stemHeight * (0.48 + sprigIndex * 0.16),
                    stem.position.z * (0.72 + sprigIndex * 0.1)
                );
                frond.rotation.y = angle + (sprigIndex - 1) * 0.9;
                frond.rotation.z = 0.32 + deterministicNoise01(tile.y, tile.x, index + sprigIndex + 61) * 0.44;
                frond.rotation.x = -0.14 + deterministicNoise01(tile.x, tile.y, index + sprigIndex + 71) * 0.28;
                plantGroup.add(frond);
            }
        }
    }

    function addWhiteThornPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const twigColor = 0x766144;
        const leafColors = [0x8aa062, 0x96ac6d, 0x7b8f57];
        const berryColor = 0xb4c8de;
        const branchCount =
            Math.max(7, Math.round((7 + plantDensity * 4) * Math.max(1.0, plantSpec.areaScale * 0.86)));
        const bushRadiusX = plantSpec.halfSpanX * (0.46 + plantDensity * 0.22);
        const bushRadiusZ = plantSpec.halfSpanZ * (0.46 + plantDensity * 0.22);

        for (let index = 0; index < branchCount; index += 1) {
            const angle =
                (index / branchCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 61) * 0.6;
            const branchHeight =
                0.18 + plantDensity * 0.12 + deterministicNoise01(tile.y, tile.x, index + 71) * 0.12;
            const branch = createBranchMesh(twigColor, 0.024, 0.011, branchHeight, 5, 0.42);
            branch.position.set(
                Math.cos(angle) * bushRadiusX * 0.16,
                branchHeight * 0.38,
                Math.sin(angle) * bushRadiusZ * 0.16
            );
            branch.rotation.z = Math.cos(angle) * 0.82;
            branch.rotation.x = -Math.sin(angle) * 0.82;
            plantGroup.add(branch);
            const branchTip = new THREE_NS.Vector3(
                branch.position.x + Math.sin(branch.rotation.z) * branchHeight * 0.42,
                branch.position.y + Math.cos(branch.rotation.z) * branchHeight * 0.34,
                branch.position.z - Math.sin(branch.rotation.x) * branchHeight * 0.42
            );
            const leafPairCount = 2 + (index % 2);
            for (let leafIndex = 0; leafIndex < leafPairCount; leafIndex += 1) {
                const leaf = createLeafMesh(
                    leafColors[(index + leafIndex) % leafColors.length],
                    0.03,
                    0.01,
                    0.075 + deterministicNoise01(tile.x, tile.y, index + leafIndex + 81) * 0.03,
                    0.94
                );
                leaf.position.copy(branchTip);
                leaf.position.x += (leafIndex - 0.5) * 0.03;
                leaf.position.y += leafIndex * 0.018;
                leaf.rotation.y = angle + (leafIndex === 0 ? 0.55 : -0.55);
                leaf.rotation.z = 0.5 + leafIndex * 0.12;
                plantGroup.add(leaf);
            }

            const berryChance = deterministicNoise01(tile.x, tile.y, index + 141);
            if (berryChance > 0.48) {
                const berry = createBerryMesh(
                    berryColor,
                    0.018 + plantDensity * 0.004,
                    0.34
                );
                berry.position.copy(branchTip);
                berry.position.y -= 0.01;
                berry.position.x += (berryChance - 0.5) * 0.05;
                plantGroup.add(berry);
            }
        }
    }

    function addRedTamariskPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const trunkColor = 0x8a6a53;
        const frondColor = 0xa7be86;
        const branchCount =
            Math.max(14, Math.round((14 + plantDensity * 8) * Math.max(1.0, plantSpec.areaScale * 0.92)));
        const shrubHeight = 0.4 + plantDensity * 0.38 + (plantSpec.areaScale - 1.0) * 0.1;

        for (let index = 0; index < branchCount; index += 1) {
            const angle =
                (index / branchCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 201) * 0.46;
            const branchLength =
                shrubHeight * (0.72 + deterministicNoise01(tile.y, tile.x, index + 211) * 0.54);
            const branch = createBranchMesh(
                trunkColor,
                0.018 + deterministicNoise01(tile.x, tile.y, index + 221) * 0.008,
                0.005,
                branchLength,
                5,
                0.7
            );
            branch.position.set(
                Math.cos(angle) * plantSpec.halfSpanX * 0.08,
                branchLength * 0.46,
                Math.sin(angle) * plantSpec.halfSpanZ * 0.08
            );
            branch.rotation.z = Math.cos(angle) * (0.26 + deterministicNoise01(tile.y, tile.x, index + 231) * 0.28);
            branch.rotation.x = -Math.sin(angle) * (0.26 + deterministicNoise01(tile.x, tile.y, index + 241) * 0.28);
            plantGroup.add(branch);

            const tuftCount = 3 + (index % 2);
            for (let tuftIndex = 0; tuftIndex < tuftCount; tuftIndex += 1) {
                const tuft = createGrassBladeMesh(
                    frondColor,
                    0.016,
                    0.12 + plantDensity * 0.06,
                    1.08
                );
                tuft.position.set(
                    branch.position.x + Math.sin(branch.rotation.z) * branchLength * (0.2 + tuftIndex * 0.18),
                    branch.position.y + branchLength * (0.08 + tuftIndex * 0.15),
                    branch.position.z - Math.sin(branch.rotation.x) * branchLength * (0.2 + tuftIndex * 0.18)
                );
                tuft.rotation.y = angle + tuftIndex * 0.7;
                tuft.rotation.z = 0.34 + deterministicNoise01(tile.x, tile.y, index + tuftIndex + 251) * 0.3;
                plantGroup.add(tuft);
            }
        }
    }

    function addNingxiaWolfberryPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const caneColor = 0x7f6a4d;
        const leafColors = [0x8fb468, 0x7f9f59, 0x9abf74];
        const fruitColor = 0xd45b39;
        const caneCount =
            Math.max(5, Math.round((5 + plantDensity * 3) * Math.max(1.0, plantSpec.areaScale * 0.82)));
        const spreadX = plantSpec.halfSpanX * (0.72 + plantDensity * 0.12);
        const spreadZ = plantSpec.halfSpanZ * (0.64 + plantDensity * 0.1);

        for (let index = 0; index < caneCount; index += 1) {
            const side = index % 2 === 0 ? -1 : 1;
            const arc = index / Math.max(caneCount - 1, 1);
            const caneCurve = new THREE_NS.CatmullRomCurve3([
                new THREE_NS.Vector3(side * spreadX * 0.12, 0.02, (arc - 0.5) * spreadZ * 0.3),
                new THREE_NS.Vector3(side * spreadX * 0.2, 0.18 + plantDensity * 0.05, (arc - 0.5) * spreadZ * 0.48),
                new THREE_NS.Vector3(side * spreadX * 0.62, 0.28 + plantDensity * 0.08, (arc - 0.5) * spreadZ * 0.62),
                new THREE_NS.Vector3(side * spreadX * (0.84 + deterministicNoise01(tile.x, tile.y, index + 301) * 0.08), 0.1 + plantDensity * 0.05, (arc - 0.5) * spreadZ)
            ]);
            const cane = new THREE_NS.Mesh(
                new THREE_NS.TubeGeometry(caneCurve, 14, 0.012 + plantDensity * 0.002, 5, false),
                createPlantMaterial(caneColor, 0.86, 0x24170f, 0.0, { windFlexibility: 0.9 })
            );
            plantGroup.add(cane);

            for (let nodeIndex = 0; nodeIndex < 4; nodeIndex += 1) {
                const t = 0.2 + nodeIndex * 0.18;
                const point = caneCurve.getPoint(t);
                const tangent = caneCurve.getTangent(t);
                const leaf = createLeafMesh(
                    leafColors[(index + nodeIndex) % leafColors.length],
                    0.035 + plantDensity * 0.012,
                    0.01,
                    0.11 + plantDensity * 0.02,
                    1.04
                );
                leaf.position.copy(point);
                leaf.rotation.y = Math.atan2(tangent.x, tangent.z) + (nodeIndex % 2 === 0 ? 0.65 : -0.65);
                leaf.rotation.z = nodeIndex % 2 === 0 ? -0.42 : 0.42;
                plantGroup.add(leaf);

                if (nodeIndex >= 1 && deterministicNoise01(tile.y, tile.x, index + nodeIndex + 331) > 0.34) {
                    const berry = createBerryMesh(fruitColor, 0.022 + plantDensity * 0.004, 0.4);
                    berry.position.copy(point);
                    berry.position.y -= 0.016;
                    berry.position.x += (nodeIndex % 2 === 0 ? -1 : 1) * 0.024;
                    plantGroup.add(berry);
                }
            }
        }
    }

    function addKorshinskPeashrubPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const branchColor = 0x7a6945;
        const leafColors = [0x8aac5f, 0x96b96a, 0x7d9d56];
        const flowerColor = 0xd2bb58;
        const branchCount =
            Math.max(8, Math.round((8 + plantDensity * 4) * Math.max(1.0, plantSpec.areaScale * 0.84)));
        for (let index = 0; index < branchCount; index += 1) {
            const angle =
                (index / branchCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 401) * 0.52;
            const branchHeight = 0.24 + plantDensity * 0.16 + deterministicNoise01(tile.y, tile.x, index + 411) * 0.14;
            const branch = createBranchMesh(branchColor, 0.024, 0.008, branchHeight, 5, 0.46);
            branch.position.set(
                Math.cos(angle) * plantSpec.halfSpanX * 0.1,
                branchHeight * 0.44,
                Math.sin(angle) * plantSpec.halfSpanZ * 0.1
            );
            branch.rotation.z = Math.cos(angle) * 0.54;
            branch.rotation.x = -Math.sin(angle) * 0.54;
            plantGroup.add(branch);

            for (let nodeIndex = 0; nodeIndex < 3; nodeIndex += 1) {
                const nodeHeight = branchHeight * (0.34 + nodeIndex * 0.2);
                const nodeX = branch.position.x + Math.sin(branch.rotation.z) * nodeHeight * 0.4;
                const nodeZ = branch.position.z - Math.sin(branch.rotation.x) * nodeHeight * 0.4;
                for (let leafletIndex = 0; leafletIndex < 3; leafletIndex += 1) {
                    const leaf = createLeafMesh(
                        leafColors[(index + leafletIndex) % leafColors.length],
                        0.024,
                        0.008,
                        0.06,
                        0.88
                    );
                    leaf.position.set(
                        nodeX + (leafletIndex - 1) * 0.02,
                        branch.position.y - branchHeight * 0.18 + nodeHeight,
                        nodeZ + (leafletIndex - 1) * 0.01
                    );
                    leaf.rotation.y = angle + (leafletIndex - 1) * 0.6;
                    leaf.rotation.z = 0.38;
                    plantGroup.add(leaf);
                }

                if (nodeIndex === 2 && deterministicNoise01(tile.x, tile.y, index + 421) > 0.58) {
                    const flower = createBerryMesh(flowerColor, 0.018, 0.28);
                    flower.position.set(nodeX, branch.position.y - branchHeight * 0.18 + nodeHeight + 0.02, nodeZ);
                    plantGroup.add(flower);
                }
            }
        }
    }

    function addJijiGrassPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const bladeColors = [0x88b274, 0x6f9a61, 0x9ac182];
        const bladeCount =
            Math.max(18, Math.round((18 + plantDensity * 24) * Math.max(1.0, plantSpec.areaScale * 0.88)));
        for (let index = 0; index < bladeCount; index += 1) {
            const angle = deterministicNoise01(tile.x, tile.y, index + 451) * Math.PI * 2;
            const radius = Math.pow(deterministicNoise01(tile.y, tile.x, index + 461), 0.72) * 0.58;
            const bladeHeight =
                0.26 + plantDensity * 0.26 + deterministicNoise01(tile.x, tile.y, index + 471) * 0.18;
            const blade = createGrassBladeMesh(
                bladeColors[index % bladeColors.length],
                0.026 + deterministicNoise01(tile.y, tile.x, index + 481) * 0.016,
                bladeHeight,
                1.28
            );
            blade.position.set(
                Math.cos(angle) * plantSpec.halfSpanX * radius,
                0,
                Math.sin(angle) * plantSpec.halfSpanZ * radius
            );
            blade.rotation.y = angle;
            blade.rotation.z = 0.22 + deterministicNoise01(tile.x, tile.y, index + 491) * 0.54;
            plantGroup.add(blade);
        }
    }

    function addSeaBuckthornPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const branchColor = 0x78664a;
        const leafColors = [0xa0b48d, 0x92a97f, 0x859a73];
        const berryColor = 0xde7a27;
        const branchCount =
            Math.max(8, Math.round((8 + plantDensity * 5) * Math.max(1.0, plantSpec.areaScale * 0.84)));
        for (let index = 0; index < branchCount; index += 1) {
            const angle =
                (index / branchCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 501) * 0.44;
            const branchHeight = 0.22 + plantDensity * 0.16 + deterministicNoise01(tile.y, tile.x, index + 511) * 0.12;
            const branch = createBranchMesh(branchColor, 0.026, 0.008, branchHeight, 5, 0.38);
            branch.position.set(
                Math.cos(angle) * plantSpec.halfSpanX * 0.08,
                branchHeight * 0.42,
                Math.sin(angle) * plantSpec.halfSpanZ * 0.08
            );
            branch.rotation.z = Math.cos(angle) * 0.72;
            branch.rotation.x = -Math.sin(angle) * 0.72;
            plantGroup.add(branch);

            for (let nodeIndex = 0; nodeIndex < 4; nodeIndex += 1) {
                const nodeHeight = branchHeight * (0.2 + nodeIndex * 0.18);
                const nodeX = branch.position.x + Math.sin(branch.rotation.z) * nodeHeight * 0.44;
                const nodeZ = branch.position.z - Math.sin(branch.rotation.x) * nodeHeight * 0.44;
                const leaf = createLeafMesh(
                    leafColors[(index + nodeIndex) % leafColors.length],
                    0.022,
                    0.007,
                    0.08,
                    0.86
                );
                leaf.position.set(nodeX, branch.position.y - branchHeight * 0.2 + nodeHeight, nodeZ);
                leaf.rotation.y = angle + (nodeIndex % 2 === 0 ? 0.52 : -0.52);
                leaf.rotation.z = nodeIndex % 2 === 0 ? 0.44 : -0.44;
                plantGroup.add(leaf);

                if (nodeIndex >= 1 && deterministicNoise01(tile.x, tile.y, index + nodeIndex + 521) > 0.36) {
                    const berryClusterCount = 1 + (nodeIndex % 2);
                    for (let berryIndex = 0; berryIndex < berryClusterCount; berryIndex += 1) {
                        const berry = createBerryMesh(berryColor, 0.02 + plantDensity * 0.004, 0.34);
                        berry.position.set(
                            nodeX + (berryIndex - 0.5) * 0.02,
                            branch.position.y - branchHeight * 0.22 + nodeHeight - 0.018 - berryIndex * 0.008,
                            nodeZ
                        );
                        plantGroup.add(berry);
                    }
                }
            }
        }
    }

    function addDesertEphedraPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const stemColor = 0x73955f;
        const stemCount =
            Math.max(10, Math.round((10 + plantDensity * 10) * Math.max(1.0, plantSpec.areaScale * 0.88)));
        for (let index = 0; index < stemCount; index += 1) {
            const angle =
                (index / stemCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 551) * 0.52;
            const stemHeight = 0.24 + plantDensity * 0.22 + deterministicNoise01(tile.y, tile.x, index + 561) * 0.16;
            const segmentCount = 3 + (index % 2);
            for (let segmentIndex = 0; segmentIndex < segmentCount; segmentIndex += 1) {
                const segmentHeight = stemHeight / segmentCount;
                const segment = createBranchMesh(
                    stemColor,
                    0.014 - segmentIndex * 0.0015,
                    Math.max(0.006, 0.011 - segmentIndex * 0.0012),
                    segmentHeight,
                    5,
                    0.98
                );
                segment.position.set(
                    Math.cos(angle) * plantSpec.halfSpanX * 0.12,
                    segmentHeight * 0.5 + segmentIndex * segmentHeight * 0.82,
                    Math.sin(angle) * plantSpec.halfSpanZ * 0.12
                );
                segment.rotation.z = Math.cos(angle) * (0.2 + segmentIndex * 0.12);
                segment.rotation.x = -Math.sin(angle) * (0.2 + segmentIndex * 0.12);
                plantGroup.add(segment);
            }
        }
    }

    function addSaxaulPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const trunkColor = 0x7d6a56;
        const twigColor = 0x8fa070;
        const trunkHeight = 0.42 + plantDensity * 0.28 + (plantSpec.areaScale - 1.0) * 0.12;
        const trunk = createBranchMesh(trunkColor, 0.08, 0.05, trunkHeight, 7, 0.18);
        trunk.position.y = trunkHeight * 0.5;
        trunk.rotation.z = 0.04;
        trunk.rotation.x = -0.05;
        plantGroup.add(trunk);

        const crownCount =
            Math.max(7, Math.round((7 + plantDensity * 4) * Math.max(1.0, plantSpec.areaScale * 0.84)));
        for (let index = 0; index < crownCount; index += 1) {
            const angle =
                (index / crownCount) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 601) * 0.5;
            const branchLength = 0.26 + plantDensity * 0.16 + deterministicNoise01(tile.y, tile.x, index + 611) * 0.12;
            const branch = createBranchMesh(trunkColor, 0.026, 0.01, branchLength, 5, 0.38);
            branch.position.set(
                Math.cos(angle) * 0.03,
                trunkHeight * (0.56 + deterministicNoise01(tile.x, tile.y, index + 621) * 0.18),
                Math.sin(angle) * 0.03
            );
            branch.rotation.z = Math.cos(angle) * 0.76;
            branch.rotation.x = -Math.sin(angle) * 0.76;
            plantGroup.add(branch);

            const twigCount = 4;
            for (let twigIndex = 0; twigIndex < twigCount; twigIndex += 1) {
                const twig = createGrassBladeMesh(
                    twigColor,
                    0.016,
                    0.12 + plantDensity * 0.04,
                    0.96
                );
                twig.position.set(
                    branch.position.x + Math.sin(branch.rotation.z) * branchLength * (0.28 + twigIndex * 0.12),
                    branch.position.y + branchLength * (0.08 + twigIndex * 0.11),
                    branch.position.z - Math.sin(branch.rotation.x) * branchLength * (0.28 + twigIndex * 0.12)
                );
                twig.rotation.y = angle + twigIndex * 0.6;
                twig.rotation.z = 0.36 + deterministicNoise01(tile.x, tile.y, index + twigIndex + 631) * 0.26;
                plantGroup.add(twig);
            }
        }
    }

    function addGenericPlant(plantGroup, plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const stem = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(
                0.03 + (plantSpec.areaScale - 1.0) * 0.005,
                0.04 + (plantSpec.areaScale - 1.0) * 0.006,
                0.26 + plantDensity * 0.18 + (plantSpec.areaScale - 1.0) * 0.08,
                6
            ),
            createPlantMaterial(0x72884d, 0.84, 0x263016, 0.01, { windFlexibility: 0.56 })
        );
        stem.position.y = 0.16 + plantDensity * 0.09;
        plantGroup.add(stem);

        for (let index = 0; index < 3; index += 1) {
            const angle =
                (index / 3) * Math.PI * 2 +
                deterministicNoise01(tile.x, tile.y, index + 151) * 0.5;
            const leaf = createLeafMesh(
                0x96b867,
                0.05 + (plantSpec.areaScale - 1.0) * 0.015,
                0.012,
                0.11 + plantDensity * 0.04 + (plantSpec.areaScale - 1.0) * 0.02,
                1.0
            );
            leaf.position.set(
                Math.cos(angle) * plantSpec.halfSpanX * 0.18,
                0.14 + index * 0.04,
                Math.sin(angle) * plantSpec.halfSpanZ * 0.18
            );
            leaf.rotation.y = angle;
            leaf.rotation.z = 0.5;
            plantGroup.add(leaf);
        }
    }

    function createPlantVisual(plantSpec) {
        const tile = plantSpec.anchorTile;
        const plantDensity = plantSpec.plantDensity;
        const plantDensityScale = smoothstep01(0.0, 1.0, plantDensity);
        const plantGroup = new THREE_NS.Group();
        const palette = resolvePlantPalette(tile);
        plantGroup.userData.plantAnchorX = tile.x;
        plantGroup.userData.plantAnchorY = tile.y;
        plantGroup.userData.plantFootprintWidth = plantSpec.footprintWidth;
        plantGroup.userData.plantFootprintHeight = plantSpec.footprintHeight;
        plantGroup.position.y = plantSpec.baseHeight + 0.02;
        if (tile.plantTypeId === 5) {
            plantGroup.scale.set(
                1.0,
                0.55 + plantDensityScale * 1.55 + (plantSpec.areaScale - 1.0) * 0.08,
                1.0
            );
            addStrawCheckerboardPlant(plantGroup, plantSpec, palette);
        } else {
            plantGroup.rotation.y = deterministicNoise01(tile.x, tile.y, tile.plantTypeId + 161) * Math.PI * 2;
            plantGroup.scale.set(
                1.0,
                0.88 + plantDensity * 0.34 + (plantSpec.areaScale - 1.0) * 0.08,
                1.0
            );

            if (tile.plantTypeId === 1) {
                addOrdosWormwoodPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 2) {
                addWhiteThornPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 3) {
                addRedTamariskPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 4) {
                addNingxiaWolfberryPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 6) {
                addKorshinskPeashrubPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 7) {
                addJijiGrassPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 8) {
                addSeaBuckthornPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 9) {
                addDesertEphedraPlant(plantGroup, plantSpec);
            } else if (tile.plantTypeId === 10) {
                addSaxaulPlant(plantGroup, plantSpec);
            } else {
                addGenericPlant(plantGroup, plantSpec);
            }
        }
        const mergedPlantVisual = collapseStaticMeshHierarchy(plantGroup);
        setPlantVisualWindStrength(mergedPlantVisual, plantSpec.localWind);
        return mergedPlantVisual;
    }

    function createStructureVisual(tile, tileHeight) {
        const structureGroup = new THREE_NS.Group();
        structureGroup.position.y = tileHeight;
        const isDeliveryBoxCrate = inventoryCache.containers.some((containerInfo) =>
            containerInfo &&
            containerInfo.containerKind === "DEVICE_STORAGE" &&
            (containerInfo.flags & inventoryStorageFlags.deliveryBox) !== 0 &&
            containerInfo.containerTileX === tile.x &&
            containerInfo.containerTileY === tile.y
        );

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
                new THREE_NS.MeshStandardMaterial({
                    color: isDeliveryBoxCrate ? 0x5f8f4d : 0x8a633d,
                    roughness: 0.92,
                    metalness: 0.02
                })
            );
            crate.position.y = 0.24;
            structureGroup.add(crate);
            if (isDeliveryBoxCrate) {
                const band = new THREE_NS.Mesh(
                    new THREE_NS.BoxGeometry(0.62, 0.08, 0.62),
                    new THREE_NS.MeshStandardMaterial({
                        color: 0x9fd37a,
                        emissive: 0x4d7d38,
                        emissiveIntensity: 0.2,
                        roughness: 0.8,
                        metalness: 0.03
                    })
                );
                band.position.y = 0.38;
                structureGroup.add(band);
            }
        } else {
            const fallback = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.5, 0.3, 0.5),
                new THREE_NS.MeshStandardMaterial({ color: 0x8f7a64, roughness: 0.9, metalness: 0.02 })
            );
            fallback.position.y = 0.16;
            structureGroup.add(fallback);
        }

        return collapseStaticMeshHierarchy(structureGroup);
    }

    function createCampVisual() {
        const campGroup = new THREE_NS.Group();

        const campPad = new THREE_NS.Mesh(
            new THREE_NS.CylinderGeometry(0.56, 0.66, 0.12, 18),
            new THREE_NS.MeshStandardMaterial({ color: 0x866341, roughness: 0.9, metalness: 0.03 })
        );
        campPad.position.set(0, 0.12, 0);
        campGroup.add(campPad);

        const tent = new THREE_NS.Mesh(
            new THREE_NS.ConeGeometry(0.38, 0.6, 4),
            new THREE_NS.MeshStandardMaterial({ color: 0xd8d1bf, roughness: 0.85, metalness: 0.02 })
        );
        tent.position.set(0, 0.48, 0);
        tent.rotation.y = Math.PI * 0.25;
        campGroup.add(tent);

        return collapseStaticMeshHierarchy(campGroup);
    }

    function computeSiteTileVisualHeight(tile) {
        const plantDensity = Math.max(0, Math.min(tile && tile.plantDensity ? tile.plantDensity : 0, 1));
        const burial = Math.max(0, Math.min(tile && tile.sandBurial ? tile.sandBurial : 0, 1));
        const tileX = tile && typeof tile.x === "number" ? tile.x : 0;
        const tileY = tile && typeof tile.y === "number" ? tile.y : 0;
        const noise = ((tileX * 17 + tileY * 29) % 7) * 0.008;
        return 0.08 + burial * 0.14 + plantDensity * 0.18 + noise;
    }

    function getSiteTileByCoord(tileMap, width, height, tileX, tileY) {
        const clampedX = Math.max(0, Math.min(Math.max(width - 1, 0), tileX));
        const clampedY = Math.max(0, Math.min(Math.max(height - 1, 0), tileY));
        return tileMap.get(clampedX + ":" + clampedY) || {
            x: clampedX,
            y: clampedY,
            terrainTypeId: 0,
            plantTypeId: 0,
            structureTypeId: 0,
            groundCoverTypeId: 0,
            plantDensity: 0,
            sandBurial: 0,
            moisture: 0,
            soilFertility: 0,
            soilSalinity: 0,
            excavationDepth: 0,
            visibleExcavationDepth: 0
        };
    }

    function readTileMeter(tile, key) {
        return clamp01(tile && typeof tile[key] === "number" ? tile[key] : 0);
    }

    function readTileProtectionValue(tile, modeName) {
        if (!tile) {
            return 0;
        }

        if (modeName === "WIND") {
            return typeof tile.windProtection === "number" ? tile.windProtection : 0;
        }
        if (modeName === "HEAT") {
            return typeof tile.heatProtection === "number" ? tile.heatProtection : 0;
        }
        if (modeName === "DUST") {
            return typeof tile.dustProtection === "number" ? tile.dustProtection : 0;
        }
        return 0;
    }

    function resolveProtectionOverlayPeakValue(tiles, modeName) {
        return (tiles || []).reduce(function (peakValue, tile) {
            return Math.max(peakValue, readTileProtectionValue(tile, modeName));
        }, 0);
    }

    function applyTerrainSurfaceShader(material) {
        material.onBeforeCompile = function (shader) {
            shader.vertexShader = shader.vertexShader.replace(
                "#include <common>",
                "#include <common>\n" +
                "attribute float terrainRoughness;\n" +
                "varying float vTerrainRoughness;\n"
            );
            shader.vertexShader = shader.vertexShader.replace(
                "#include <begin_vertex>",
                "#include <begin_vertex>\n" +
                "vTerrainRoughness = terrainRoughness;\n"
            );
            shader.fragmentShader = shader.fragmentShader.replace(
                "#include <common>",
                "#include <common>\n" +
                "varying float vTerrainRoughness;\n"
            );
            shader.fragmentShader = shader.fragmentShader.replace(
                "float roughnessFactor = roughness;",
                "float roughnessFactor = roughness * clamp(vTerrainRoughness, 0.18, 1.0);"
            );
        };
        material.customProgramCacheKey = function () {
            return "terrain-surface-v1";
        };
        return material;
    }

    function sampleSiteTileScalar(tileMap, width, height, tileX, tileY, readValue) {
        const maxTileX = Math.max(width - 1, 0);
        const maxTileY = Math.max(height - 1, 0);
        const clampedTileX = Math.max(0, Math.min(maxTileX, tileX));
        const clampedTileY = Math.max(0, Math.min(maxTileY, tileY));
        const minX = Math.floor(clampedTileX);
        const minY = Math.floor(clampedTileY);
        const maxX = Math.min(maxTileX, minX + 1);
        const maxY = Math.min(maxTileY, minY + 1);
        const blendX = clampedTileX - minX;
        const blendY = clampedTileY - minY;
        const value00 = readValue(getSiteTileByCoord(tileMap, width, height, minX, minY));
        const value10 = readValue(getSiteTileByCoord(tileMap, width, height, maxX, minY));
        const value01 = readValue(getSiteTileByCoord(tileMap, width, height, minX, maxY));
        const value11 = readValue(getSiteTileByCoord(tileMap, width, height, maxX, maxY));
        const top = lerp(value00, value10, blendX);
        const bottom = lerp(value01, value11, blendX);
        return lerp(top, bottom, blendY);
    }

    function createTilePickMesh(tile, offsetX, offsetZ, tileHeight) {
        const pickMesh = new THREE_NS.Mesh(
            new THREE_NS.PlaneGeometry(0.98, 0.98),
            new THREE_NS.MeshBasicMaterial({
                color: 0xffffff,
                side: THREE_NS.DoubleSide,
                transparent: true,
                opacity: 0,
                depthWrite: false
            })
        );
        pickMesh.rotation.x = -Math.PI / 2;
        pickMesh.position.set(tile.x - offsetX, tileHeight + 0.025, tile.y - offsetZ);
        pickMesh.userData = {
            tileX: tile.x,
            tileY: tile.y
        };
        return pickMesh;
    }

    function createSiteTerrainVisual(siteBootstrap, offsetX, offsetZ, width, height) {
        const terrainGroup = new THREE_NS.Group();
        const tilePickables = [];
        const tileMap = buildSiteTileMap(siteBootstrap.tiles);
        const terrainWidth = Math.max(width - 1, 1);
        const terrainHeight = Math.max(height - 1, 1);
        const terrainSegmentsX = Math.max(width * 5, 4);
        const terrainSegmentsY = Math.max(height * 5, 4);
        const terrainGeometry = new THREE_NS.PlaneGeometry(
            terrainWidth,
            terrainHeight,
            terrainSegmentsX,
            terrainSegmentsY
        );
        const terrainPositions = terrainGeometry.attributes.position;
        const terrainColors = new Float32Array(terrainPositions.count * 3);
        const terrainRoughness = new Float32Array(terrainPositions.count);
        const duneShadowColor = new THREE_NS.Color(0xb98652);
        const duneBaseColor = new THREE_NS.Color(0xd2aa74);
        const duneHighlightColor = new THREE_NS.Color(0xe8cda0);
        const hardpackColor = new THREE_NS.Color(0xb18a5b);
        const plantTintColor = new THREE_NS.Color(0xa6a06d);
        const fertileSoilColor = new THREE_NS.Color(0x7d6040);
        const wetSoilColor = new THREE_NS.Color(0x667257);
        const salinityTintColor = new THREE_NS.Color(0xe3ddd0);
        const scratchColor = new THREE_NS.Color();

        for (let vertexIndex = 0; vertexIndex < terrainPositions.count; vertexIndex += 1) {
            const localX = terrainPositions.getX(vertexIndex);
            const localZ = terrainPositions.getY(vertexIndex);
            const sampleTileX = localX + offsetX;
            const sampleTileY = localZ + offsetZ;
            const baseTileHeight = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => computeSiteTileVisualHeight(tile)
            );
            const burial = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => readTileMeter(tile, "sandBurial")
            );
            const plantCoverage = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => tile && tile.plantTypeId !== 0 ? 1.0 : clamp01(tile && tile.plantDensity ? tile.plantDensity : 0)
            );
            const hardpackBlend = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => tile && tile.terrainTypeId === highwayTerrainTypeId ? 1.0 : 0.0
            );
            const moisture = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => readTileMeter(tile, "moisture")
            );
            const soilFertility = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => readTileMeter(tile, "soilFertility")
            );
            const soilSalinity = sampleSiteTileScalar(
                tileMap,
                width,
                height,
                sampleTileX,
                sampleTileY,
                (tile) => readTileMeter(tile, "soilSalinity")
            );
            const macroNoise = deterministicNoise01(localX * 0.22, localZ * 0.22, 801);
            const rippleNoise = deterministicNoise01(localZ * 0.94, localX * 0.94, 809);
            const duneWave =
                Math.sin(localX * 0.72 + localZ * 0.38 + macroNoise * Math.PI * 2.0) * 0.05;
            const duneLift =
                duneWave +
                (macroNoise - 0.5) * 0.06 +
                (rippleNoise - 0.5) * 0.025;
            const terrainHeight = Math.max(0.025, baseTileHeight + duneLift * 0.35);
            const highlightMix = clamp01(
                0.38 +
                burial * 0.22 +
                (duneWave * 0.5 + 0.5) * 0.28 +
                macroNoise * 0.12
            );
            const ecologicalBlendScale = 1.0 - hardpackBlend * 0.72;
            const fertileBlend = clamp01(
                (soilFertility * 0.82 + moisture * 0.18 - burial * 0.1) * ecologicalBlendScale
            );
            const wetBlend = clamp01(
                (moisture * 0.78 + soilFertility * 0.12 - soilSalinity * 0.08) * ecologicalBlendScale
            );
            const mineralCrust = clamp01(
                soilSalinity *
                (1.0 - soilFertility * 0.6) *
                (1.0 - moisture * 0.45) *
                (0.58 + macroNoise * 0.28) *
                ecologicalBlendScale
            );
            const surfaceRoughness = Math.max(
                0.36,
                Math.min(
                    1.0,
                    0.96 -
                        moisture * 0.34 -
                        soilFertility * 0.06 +
                        burial * 0.08 +
                        mineralCrust * 0.1 +
                        hardpackBlend * 0.04
                )
            );

            terrainPositions.setZ(vertexIndex, terrainHeight);
            scratchColor.copy(duneShadowColor).lerp(duneBaseColor, highlightMix);
            scratchColor.lerp(duneHighlightColor, clamp01((macroNoise - 0.36) * 0.65 + burial * 0.26));
            scratchColor.lerp(fertileSoilColor, fertileBlend * 0.62);
            scratchColor.lerp(wetSoilColor, wetBlend * 0.46);
            scratchColor.lerp(salinityTintColor, mineralCrust * 0.34);
            scratchColor.lerp(hardpackColor, hardpackBlend * 0.78);
            scratchColor.lerp(plantTintColor, plantCoverage * 0.16);
            terrainColors[vertexIndex * 3] = scratchColor.r;
            terrainColors[vertexIndex * 3 + 1] = scratchColor.g;
            terrainColors[vertexIndex * 3 + 2] = scratchColor.b;
            terrainRoughness[vertexIndex] = surfaceRoughness;
        }

        terrainGeometry.setAttribute("color", new THREE_NS.Float32BufferAttribute(terrainColors, 3));
        terrainGeometry.setAttribute("terrainRoughness", new THREE_NS.Float32BufferAttribute(terrainRoughness, 1));
        terrainGeometry.computeVertexNormals();

        const terrainMesh = new THREE_NS.Mesh(
            terrainGeometry,
            applyTerrainSurfaceShader(new THREE_NS.MeshStandardMaterial({
                vertexColors: true,
                roughness: 1.0,
                metalness: 0.01
            }))
        );
        terrainMesh.rotation.x = -Math.PI / 2;
        terrainMesh.position.y = 0;
        terrainGroup.add(terrainMesh);

        siteBootstrap.tiles.forEach((tile) => {
            const tileHeight = computeSiteTileVisualHeight(tile);
            const pickMesh = createTilePickMesh(tile, offsetX, offsetZ, tileHeight);
            terrainGroup.add(pickMesh);
            tilePickables.push(pickMesh);
        });

        return {
            group: terrainGroup,
            tilePickables: tilePickables
        };
    }

    function createPlacementGridVisual(siteBootstrap, offsetX, offsetZ) {
        const linePositions = [];

        (siteBootstrap.tiles || []).forEach((tile) => {
            const tileHeight = computeSiteTileVisualHeight(tile) + 0.04;
            const centerX = tile.x - offsetX;
            const centerZ = tile.y - offsetZ;
            const minX = centerX - 0.45;
            const maxX = centerX + 0.45;
            const minZ = centerZ - 0.45;
            const maxZ = centerZ + 0.45;

            linePositions.push(
                minX, tileHeight, minZ,
                maxX, tileHeight, minZ,
                maxX, tileHeight, minZ,
                maxX, tileHeight, maxZ,
                maxX, tileHeight, maxZ,
                minX, tileHeight, maxZ,
                minX, tileHeight, maxZ,
                minX, tileHeight, minZ
            );
        });

        const gridGeometry = new THREE_NS.BufferGeometry();
        gridGeometry.setAttribute(
            "position",
            new THREE_NS.Float32BufferAttribute(linePositions, 3)
        );

        const gridLines = new THREE_NS.LineSegments(
            gridGeometry,
            new THREE_NS.LineBasicMaterial({
                color: 0x6b5436,
                transparent: true,
                opacity: 0.5
            })
        );
        gridLines.visible = false;
        return gridLines;
    }

    function disposePlacementPreviewGroup(cache) {
        if (!cache || !cache.placementPreviewGroup) {
            return;
        }

        const previewGroup = cache.placementPreviewGroup;
        worldGroup.remove(previewGroup);
        previewGroup.traverse((node) => {
            if (node.geometry && typeof node.geometry.dispose === "function") {
                node.geometry.dispose();
            }
            if (node.material && typeof node.material.dispose === "function") {
                node.material.dispose();
            }
        });
        cache.placementPreviewGroup = null;
    }

    function disposeProtectionOverlayGroup(cache) {
        if (!cache || !cache.protectionOverlayGroup) {
            return;
        }

        const overlayGroup = cache.protectionOverlayGroup;
        worldGroup.remove(overlayGroup);
        overlayGroup.traverse((node) => {
            if (node.geometry && typeof node.geometry.dispose === "function") {
                node.geometry.dispose();
            }
            if (node.material && typeof node.material.dispose === "function") {
                node.material.dispose();
            }
        });
        cache.protectionOverlayGroup = null;
    }

    function disposeExcavationMarksGroup(cache) {
        if (!cache || !cache.excavationMarksGroup) {
            return;
        }

        const markGroup = cache.excavationMarksGroup;
        worldGroup.remove(markGroup);
        markGroup.traverse((node) => {
            if (node.geometry && typeof node.geometry.dispose === "function") {
                node.geometry.dispose();
            }
            if (node.material && typeof node.material.dispose === "function") {
                node.material.dispose();
            }
        });
        cache.excavationMarksGroup = null;
    }

    function addExcavationMarkPlate(group, centerX, centerY, centerZ, color, depth) {
        const plateColor = color.clone().lerp(new THREE_NS.Color(0x1d140c), 0.72);
        const plate = new THREE_NS.Mesh(
            new THREE_NS.PlaneGeometry(0.92, 0.92),
            new THREE_NS.MeshStandardMaterial({
                color: plateColor,
                emissive: color.clone().multiplyScalar(0.1),
                emissiveIntensity: 0.35,
                roughness: 0.78,
                metalness: 0.04,
                transparent: true,
                opacity: depth >= 3 ? 0.52 : (depth === 2 ? 0.42 : 0.34),
                side: THREE_NS.DoubleSide
            })
        );
        plate.rotation.x = -Math.PI / 2;
        plate.position.set(centerX, centerY, centerZ);
        group.add(plate);
    }

    function addExcavationOutline(group, centerX, centerY, centerZ, color, depth) {
        const halfExtent = 0.46;
        const geometry = new THREE_NS.BufferGeometry().setFromPoints([
            new THREE_NS.Vector3(centerX - halfExtent, centerY, centerZ - halfExtent),
            new THREE_NS.Vector3(centerX + halfExtent, centerY, centerZ - halfExtent),
            new THREE_NS.Vector3(centerX + halfExtent, centerY, centerZ + halfExtent),
            new THREE_NS.Vector3(centerX - halfExtent, centerY, centerZ + halfExtent)
        ]);
        const outline = new THREE_NS.LineLoop(
            geometry,
            new THREE_NS.LineBasicMaterial({
                color: color,
                transparent: true,
                opacity: depth >= 3 ? 0.98 : (depth === 2 ? 0.9 : 0.82)
            })
        );
        group.add(outline);
    }

    function addExcavationMarkStrips(group, centerX, centerY, centerZ, color, depth) {
        const stripAngles = depth >= 3
            ? [0.0, Math.PI * 0.5, 0.78]
            : (depth === 2 ? [0.0, Math.PI * 0.5] : [0.55]);
        const stripLength = depth >= 3 ? 0.72 : (depth === 2 ? 0.66 : 0.58);
        const stripWidth = depth >= 3 ? 0.12 : (depth === 2 ? 0.115 : 0.13);
        stripAngles.forEach((angle, index) => {
            const mesh = new THREE_NS.Mesh(
                new THREE_NS.PlaneGeometry(stripLength, stripWidth),
                new THREE_NS.MeshStandardMaterial({
                    color: color,
                    emissive: color.clone().multiplyScalar(0.32),
                    emissiveIntensity: 0.62,
                    roughness: 0.34,
                    metalness: 0.02,
                    transparent: true,
                    opacity: depth >= 3 ? 0.94 : (depth === 2 ? 0.88 : 0.8),
                    side: THREE_NS.DoubleSide
                })
            );
            mesh.rotation.x = -Math.PI / 2;
            mesh.rotation.z = angle;
            mesh.position.set(centerX, centerY + 0.002 + index * 0.003, centerZ);
            group.add(mesh);
        });
    }

    function addExcavationCenterBadge(group, centerX, centerY, centerZ, color, depth) {
        const outerRadius = depth >= 3 ? 0.16 : (depth === 2 ? 0.12 : 0.095);
        const outer = new THREE_NS.Mesh(
            depth >= 3
                ? new THREE_NS.RingGeometry(outerRadius * 0.55, outerRadius, 24)
                : new THREE_NS.CircleGeometry(outerRadius, 24),
            new THREE_NS.MeshStandardMaterial({
                color: depth >= 3 ? color : color.clone().offsetHSL(0, 0, 0.08),
                emissive: color.clone().multiplyScalar(0.28),
                emissiveIntensity: 0.55,
                roughness: 0.28,
                metalness: 0.04,
                transparent: true,
                opacity: depth >= 3 ? 0.96 : 0.88,
                side: THREE_NS.DoubleSide
            })
        );
        outer.rotation.x = -Math.PI / 2;
        outer.position.set(centerX, centerY + 0.008, centerZ);
        group.add(outer);

        if (depth >= 2) {
            const core = new THREE_NS.Mesh(
                new THREE_NS.CircleGeometry(depth >= 3 ? 0.045 : 0.038, 18),
                new THREE_NS.MeshStandardMaterial({
                    color: depth >= 3 ? 0x17110b : 0x24170e,
                    emissive: color.clone().multiplyScalar(0.12),
                    emissiveIntensity: 0.4,
                    roughness: 0.24,
                    metalness: 0.0,
                    transparent: true,
                    opacity: 0.92,
                    side: THREE_NS.DoubleSide
                })
            );
            core.rotation.x = -Math.PI / 2;
            core.position.set(centerX, centerY + 0.01, centerZ);
            group.add(core);
        }
    }

    function addExcavationCornerTicks(group, centerX, centerY, centerZ, color, depth) {
        if (depth < 2) {
            return;
        }

        const offsets = [
            [-0.31, -0.31],
            [0.31, -0.31],
            [0.31, 0.31],
            [-0.31, 0.31]
        ];
        offsets.forEach((offset, index) => {
            const tick = new THREE_NS.Mesh(
                new THREE_NS.PlaneGeometry(depth >= 3 ? 0.16 : 0.13, 0.04),
                new THREE_NS.MeshStandardMaterial({
                    color: color,
                    emissive: color.clone().multiplyScalar(0.24),
                    emissiveIntensity: 0.48,
                    roughness: 0.32,
                    metalness: 0.02,
                    transparent: true,
                    opacity: depth >= 3 ? 0.92 : 0.82,
                    side: THREE_NS.DoubleSide
                })
            );
            tick.rotation.x = -Math.PI / 2;
            tick.rotation.z = index * (Math.PI * 0.5);
            tick.position.set(centerX + offset[0], centerY + 0.006, centerZ + offset[1]);
            group.add(tick);
        });
    }

    function updateSiteExcavationMarksVisual(siteBootstrap, siteAction) {
        if (!siteSceneCache || !worldGroup) {
            return;
        }

        disposeExcavationMarksGroup(siteSceneCache);
        const siteTiles = siteBootstrap && Array.isArray(siteBootstrap.tiles)
            ? siteBootstrap.tiles
            : [];
        const markGroup = new THREE_NS.Group();
        const roughColor = new THREE_NS.Color(0xd37b38);
        const carefulColor = new THREE_NS.Color(0x58b8d7);
        const thoroughColor = new THREE_NS.Color(0xf0d67a);
        const pendingColor = new THREE_NS.Color(0xff5f2e);
        const activeExcavationTile = getActiveExcavationTile(siteAction);

        siteTiles.forEach((tile) => {
            const visibleDepth = getVisibleExcavationDepth(tile);
            const isActiveExcavationTarget =
                !!activeExcavationTile &&
                activeExcavationTile.x === tile.x &&
                activeExcavationTile.y === tile.y;
            if (visibleDepth <= 0 && !isActiveExcavationTarget) {
                return;
            }

            const tileHeight = computeSiteTileVisualHeight(tile);
            const centerX = tile.x - siteSceneCache.offsetX;
            const centerZ = tile.y - siteSceneCache.offsetZ;
            const color = isActiveExcavationTarget
                ? pendingColor
                : (visibleDepth >= 3
                    ? thoroughColor
                    : (visibleDepth === 2 ? carefulColor : roughColor));
            const displayDepth = visibleDepth > 0 ? visibleDepth : 1;
            const markHeight = tileHeight + 0.07;
            addExcavationMarkPlate(markGroup, centerX, markHeight, centerZ, color, displayDepth);
            addExcavationOutline(markGroup, centerX, markHeight + 0.001, centerZ, color, displayDepth);
            addExcavationMarkStrips(markGroup, centerX, markHeight + 0.002, centerZ, color, displayDepth);
            addExcavationCenterBadge(markGroup, centerX, markHeight, centerZ, color, displayDepth);
            addExcavationCornerTicks(markGroup, centerX, markHeight, centerZ, color, displayDepth);
        });

        worldGroup.add(markGroup);
        siteSceneCache.excavationMarksGroup = markGroup;
    }

    function updateSitePlacementPreviewVisual(state) {
        if (!siteSceneCache || !worldGroup) {
            return;
        }

        disposePlacementPreviewGroup(siteSceneCache);

        const preview = getPlacementPreview(state);
        const placementModeActive = !!(preview && (preview.flags & 1) !== 0);
        if (siteSceneCache.placementGridGroup) {
            siteSceneCache.placementGridGroup.visible = placementModeActive;
        }
        if (!placementModeActive) {
            return;
        }

        const footprintWidth = Math.max(1, preview.footprintWidth || 1);
        const footprintHeight = Math.max(1, preview.footprintHeight || 1);
        const previewGroup = new THREE_NS.Group();
        const blockedMask = Number(preview.blockedMask || 0);

        for (let offsetY = 0; offsetY < footprintHeight; offsetY += 1) {
            for (let offsetX = 0; offsetX < footprintWidth; offsetX += 1) {
                const tileIndex = offsetY * footprintWidth + offsetX;
                const blocked =
                    Math.floor(blockedMask / Math.pow(2, tileIndex)) % 2 >= 1;
                const tileX = (preview.tileX || 0) + offsetX;
                const tileY = (preview.tileY || 0) + offsetY;
                const tile = getTileSnapshot(state, tileX, tileY) || {
                    x: tileX,
                    y: tileY,
                    plantDensity: 0,
                    sandBurial: 0
                };
                const tileHeight = computeSiteTileVisualHeight(tile);
                const mesh = new THREE_NS.Mesh(
                    new THREE_NS.PlaneGeometry(0.9, 0.9),
                    new THREE_NS.MeshStandardMaterial({
                        color: blocked ? 0xbd5f50 : 0x5f9f6b,
                        emissive: blocked ? 0x4a120c : 0x123d1a,
                        emissiveIntensity: 0.65,
                        roughness: 0.38,
                        metalness: 0.02,
                        transparent: true,
                        opacity: blocked ? 0.82 : 0.62,
                        side: THREE_NS.DoubleSide
                    })
                );
                mesh.rotation.x = -Math.PI / 2;
                mesh.position.set(
                    tileX - siteSceneCache.offsetX,
                    tileHeight + 0.075,
                    tileY - siteSceneCache.offsetZ
                );
                previewGroup.add(mesh);
            }
        }

        worldGroup.add(previewGroup);
        siteSceneCache.placementPreviewGroup = previewGroup;
    }

    function updateSiteProtectionOverlayVisual(state) {
        if (!siteSceneCache || !worldGroup) {
            return;
        }

        disposeProtectionOverlayGroup(siteSceneCache);

        const protectionOverlay = getProtectionOverlayState(state);
        const modeName = protectionOverlay && typeof protectionOverlay.mode === "string"
            ? protectionOverlay.mode
            : "NONE";
        const overlayActive = modeName !== "NONE";

        if (siteSceneCache.placementGridGroup) {
            siteSceneCache.placementGridGroup.visible =
                overlayActive || !!(getPlacementPreview(state) && (getPlacementPreview(state).flags & 1) !== 0);
        }
        if (!overlayActive) {
            return;
        }

        const overlayGroup = new THREE_NS.Group();
        const lowColor = new THREE_NS.Color(0xc95649);
        const highColor = new THREE_NS.Color(0x59b36c);
        const siteBootstrap = getSiteBootstrap(state);
        const siteTiles = siteBootstrap && Array.isArray(siteBootstrap.tiles)
            ? siteBootstrap.tiles
            : [];
        const tileMap = buildSiteTileMap(siteTiles);
        const scratchColor = new THREE_NS.Color();

        siteTiles.forEach((tile) => {
            const tileSnapshot = getSiteTileByCoord(
                tileMap,
                siteSceneCache.width,
                siteSceneCache.height,
                tile.x,
                tile.y
            );
            const normalizedProtection =
                clamp01(readTileProtectionValue(tileSnapshot, modeName) / 100.0);
            scratchColor.copy(lowColor).lerp(highColor, normalizedProtection);

            const tileHeight = computeSiteTileVisualHeight(tileSnapshot);
            const mesh = new THREE_NS.Mesh(
                new THREE_NS.PlaneGeometry(0.92, 0.92),
                new THREE_NS.MeshStandardMaterial({
                    color: scratchColor.clone(),
                    emissive: scratchColor.clone().multiplyScalar(0.3),
                    emissiveIntensity: 0.5,
                    roughness: 0.34,
                    metalness: 0.02,
                    transparent: true,
                    opacity: 0.62,
                    side: THREE_NS.DoubleSide
                })
            );
            mesh.rotation.x = -Math.PI / 2;
            mesh.position.set(
                tile.x - siteSceneCache.offsetX,
                tileHeight + 0.055,
                tile.y - siteSceneCache.offsetZ
            );
            overlayGroup.add(mesh);
        });

        worldGroup.add(overlayGroup);
        siteSceneCache.protectionOverlayGroup = overlayGroup;
    }

    function rebuildStaticSiteScene(siteBootstrap, siteAction, offsetX, offsetZ, width, height, previousCache, bootstrapSignature) {
        const carriedWitheringAlerts = cloneActiveWitheringAlerts(previousCache, animationTimeSeconds);
        const triggeredWitheringAlerts = collectWitheringAlertTriggers(
            previousCache ? previousCache.sourceBootstrap : null,
            siteBootstrap
        );
        const reusableWindField =
            previousCache &&
            previousCache.windField &&
            previousCache.siteId === siteBootstrap.siteId &&
            previousCache.width === width &&
            previousCache.height === height
                ? previousCache.windField
                : null;
        if (reusableWindField && reusableWindField.group && reusableWindField.group.parent === worldGroup) {
            worldGroup.remove(reusableWindField.group);
        }
        clearWorld();
        currentSceneKind = "SITE_ACTIVE";
        scene.background = new THREE_NS.Color(0xe7d3b0);
        const cameraOrbitState = buildSiteCameraOrbitState(siteBootstrap, width, height, previousCache);

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
        const terrainVisual = createSiteTerrainVisual(siteBootstrap, offsetX, offsetZ, width, height);
        worldGroup.add(terrainVisual.group);
        const placementGridGroup = createPlacementGridVisual(siteBootstrap, offsetX, offsetZ);
        worldGroup.add(placementGridGroup);
        const witherAlertGroup = new THREE_NS.Group();
        worldGroup.add(witherAlertGroup);
        const tilePickables = terrainVisual.tilePickables;
        const plantVisualSpecs = collectPlantVisualSpecs(siteBootstrap.tiles);
        const plantVisuals = [];

        siteBootstrap.tiles.forEach((tile) => {
            const tileHeight = computeSiteTileVisualHeight(tile);

            if (tile.structureTypeId !== 0) {
                const structureVisual = createStructureVisual(tile, tileHeight);
                structureVisual.position.x = tile.x - offsetX;
                structureVisual.position.z = tile.y - offsetZ;
                worldGroup.add(structureVisual);
            }
        });

        plantVisualSpecs.forEach((plantSpec) => {
            const plantVisual = createPlantVisual(plantSpec);
            plantVisual.position.x = plantSpec.centerX - offsetX;
            plantVisual.position.z = plantSpec.centerZ - offsetZ;
            worldGroup.add(plantVisual);
            plantVisuals.push(plantVisual);
        });

        if (siteBootstrap.camp) {
            const campX = siteBootstrap.camp.tileX - offsetX;
            const campZ = siteBootstrap.camp.tileY - offsetZ;
            const campVisual = createCampVisual();
            campVisual.position.set(campX, 0.0, campZ);
            worldGroup.add(campVisual);
        }

        const workerVisual = createHumanoidWorker();
        const workerGroup = workerVisual.group;
        worldGroup.add(workerGroup);

        const windField = reusableWindField || createWindFieldVisual(width, height);
        worldGroup.add(windField.group);

        siteSceneCache = {
            siteId: siteBootstrap.siteId,
            bootstrapSignature: bootstrapSignature,
            sourceBootstrap: siteBootstrap,
            offsetX: offsetX,
            offsetZ: offsetZ,
            width: width,
            height: height,
            tilePickables: tilePickables,
            plantVisuals: plantVisuals,
            workerGroup: workerGroup,
            workerRig: workerVisual.rig,
            windField: windField,
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
            placementGridGroup: placementGridGroup,
            placementPreviewGroup: null,
            protectionOverlayGroup: null,
            excavationMarksGroup: null,
            witherAlertGroup: witherAlertGroup,
            witherAlerts: new Map(),
            cameraTargetX: 0,
            cameraTargetZ: 0,
            cameraOrbitGroundDistance: cameraOrbitState.groundDistance,
            cameraOrbitOffsetX: cameraOrbitState.offsetX,
            cameraOrbitOffsetY: cameraOrbitState.offsetY,
            cameraOrbitOffsetZ: cameraOrbitState.offsetZ
        };

        restoreWitheringAlerts(siteSceneCache, carriedWitheringAlerts);
        updateSiteExcavationMarksVisual(siteBootstrap, siteAction);
        triggerWitheringAlerts(siteSceneCache, triggeredWitheringAlerts, animationTimeSeconds);
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
        const densityOnlyWitheringAlerts =
            bootstrapReferenceChanged && siteSceneCache
                ? collectWitheringAlertTriggers(siteSceneCache.sourceBootstrap, siteBootstrap)
                : [];

        const needsStaticRebuild =
            currentSceneKind !== "SITE_ACTIVE" ||
            !siteSceneCache ||
            siteSceneCache.siteId !== siteBootstrap.siteId ||
            siteSceneCache.width !== width ||
            siteSceneCache.height !== height ||
            (bootstrapReferenceChanged && siteSceneCache.bootstrapSignature !== bootstrapSignature);

        if (needsStaticRebuild) {
            applySiteCameraBootstrapTransform(
                siteBootstrap,
                siteState,
                width,
                height,
                offsetX,
                offsetZ,
                previousSiteSceneCache);
            sendSiteControlState();
            rebuildStaticSiteScene(
                siteBootstrap,
                getSiteAction(state),
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

        if (bootstrapReferenceChanged && !needsStaticRebuild) {
            siteSceneCache.sourceBootstrap = siteBootstrap;
            updateSitePlantVisualEcology(siteSceneCache, siteBootstrap);
            updateSiteExcavationMarksVisual(siteBootstrap, getSiteAction(state));
            triggerWitheringAlerts(siteSceneCache, densityOnlyWitheringAlerts, animationTimeSeconds);
        }

        updateSiteWorkerTargetFromState(siteState, state.frameNumber || 0);
        updateSitePlacementPreviewVisual(state);
        updateSiteProtectionOverlayVisual(state);

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

    function renderState(state, options) {
        try {
            latestState = state;
            updateOverlay(state, options);
            applyWindOverlay(state);
            rebuildWorld(state);
            if (state.appState === "SITE_ACTIVE" &&
                tileContextMenuState &&
                !getSiteProtectionSelectorSetup(state) &&
                !isProtectionOverlayActive(state) &&
                !isPlacementModeActive(state)) {
                renderTileContextMenu();
            } else if (tileContextMenuState) {
                closeTileContextMenu();
            }
        } catch (error) {
            reportViewerError("renderState", error);
            throw error;
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

    function resetFpsChipSamples() {
        fpsSampleFrameCount = 0;
        fpsSampleElapsedSeconds = 0.0;
        fpsSampleWebWorkMilliseconds = 0.0;
        framePerfSampleCount = 0;
        framePerfSampleVisualHostMilliseconds = 0.0;
        framePerfSampleGameplayDllMilliseconds = 0.0;
    }

    function formatFramePerfMilliseconds(value) {
        if (!Number.isFinite(value) || value < 0.0) {
            return "-- ms";
        }

        return value.toFixed(2) + " ms";
    }

    function normalizeFramePerfSample(payload) {
        if (!payload || typeof payload !== "object") {
            return null;
        }

        const visualHostMs = Number(payload.visualHostMs);
        const gameplayDllMs = Number(payload.gameplayDllMs);
        return {
            frameNumber: typeof payload.frameNumber === "number" ? payload.frameNumber : 0,
            visualHostMs: Number.isFinite(visualHostMs) ? Math.max(visualHostMs, 0.0) : NaN,
            gameplayDllMs: Number.isFinite(gameplayDllMs) ? Math.max(gameplayDllMs, 0.0) : NaN
        };
    }

    function recordFramePerfSample(sample) {
        if (!sample) {
            return;
        }

        latestFramePerfSample = sample;
        latestFramePerfSampleTimestampMs = performance.now();
        if (!Number.isFinite(sample.visualHostMs) || !Number.isFinite(sample.gameplayDllMs)) {
            return;
        }

        framePerfSampleCount += 1;
        framePerfSampleVisualHostMilliseconds += sample.visualHostMs;
        framePerfSampleGameplayDllMilliseconds += sample.gameplayDllMs;
    }

    function updateFpsChip(frameDeltaSeconds, webFrameWorkMilliseconds) {
        if (!fpsChip || !Number.isFinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0) {
            return;
        }

        // Ignore long tab stalls so the readout keeps reflecting active rendering performance.
        if (frameDeltaSeconds > 0.5) {
            resetFpsChipSamples();
            fpsChip.textContent = "Visual Adapter\nFPS --\nWeb -- ms\nHost -- ms\nDLL -- ms";
            return;
        }

        fpsSampleFrameCount += 1;
        fpsSampleElapsedSeconds += frameDeltaSeconds;
        fpsSampleWebWorkMilliseconds +=
            Number.isFinite(webFrameWorkMilliseconds) && webFrameWorkMilliseconds >= 0.0
                ? webFrameWorkMilliseconds
                : 0.0;
        if (fpsSampleElapsedSeconds < 0.25) {
            return;
        }

        const fps = fpsSampleFrameCount / fpsSampleElapsedSeconds;
        const webMs = fpsSampleWebWorkMilliseconds / fpsSampleFrameCount;
        let visualHostMs = NaN;
        let gameplayDllMs = NaN;
        if (framePerfSampleCount > 0) {
            visualHostMs = framePerfSampleVisualHostMilliseconds / framePerfSampleCount;
            gameplayDllMs = framePerfSampleGameplayDllMilliseconds / framePerfSampleCount;
        } else if (
            latestFramePerfSample &&
            (performance.now() - latestFramePerfSampleTimestampMs) < 1000.0
        ) {
            visualHostMs = latestFramePerfSample.visualHostMs;
            gameplayDllMs = latestFramePerfSample.gameplayDllMs;
        }

        fpsChip.textContent =
            "Visual Adapter\nFPS " + Math.round(fps) +
            "\nWeb " + formatFramePerfMilliseconds(webMs) +
            "\nHost " + formatFramePerfMilliseconds(visualHostMs) +
            "\nDLL " + formatFramePerfMilliseconds(gameplayDllMs);
        resetFpsChipSamples();
    }

    function animate() {
        requestAnimationFrame(animate);

        const frameWorkStartMs = performance.now();
        const rawDeltaSeconds = animationClock.getDelta();
        const deltaSeconds = Math.min(rawDeltaSeconds, 0.1);
        animationTimeSeconds += deltaSeconds;
        const elapsed = animationTimeSeconds;
        sendSiteControlState();
        updateSmoothedWeatherVisualResponse(latestState, deltaSeconds);
        updateAudioMix(latestState);
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

        if (siteSceneCache && siteSceneCache.windField) {
            updateWindFieldVisual(siteSceneCache, latestState, elapsed);
        }
        updateWitheringAlerts(siteSceneCache, elapsed);

        updatePlantWindShaderUniforms(latestState, elapsed);
        fitRenderer();
        renderSiteHudChrome(latestState);
        renderActionProgressBar(latestState);
        renderPlacementFailureToast();
        updateDustOverlay(latestState, elapsed);
        renderSceneWithWeatherDistortion(latestState, elapsed);
        updateFpsChip(rawDeltaSeconds, performance.now() - frameWorkStartMs);
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
        if (!Array.isArray(state.recentOneShotCues)) {
            state.recentOneShotCues = [];
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
            siteAction: false,
            placementPreview: false,
            placementFailure: false,
            protectionOverlay: false,
            modifiers: false,
            audioCues: false
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
            if (field === "recentOneShotCues") {
                lightweightParts.audioCues = true;
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
                if (siteField === "placementPreview") {
                    lightweightParts.placementPreview = true;
                    continue;
                }
                if (siteField === "placementFailure") {
                    lightweightParts.placementFailure = true;
                    continue;
                }
                if (siteField === "protectionOverlay") {
                    lightweightParts.protectionOverlay = true;
                    continue;
                }
                if (siteField === "activeModifiers") {
                    lightweightParts.modifiers = true;
                    continue;
                }
                return null;
            }
        }

        if (!lightweightParts.worker &&
            !lightweightParts.weather &&
            !lightweightParts.hud &&
            !lightweightParts.siteAction &&
            !lightweightParts.placementPreview &&
            !lightweightParts.placementFailure &&
            !lightweightParts.protectionOverlay &&
            !lightweightParts.modifiers &&
            !lightweightParts.audioCues) {
            return null;
        }

        return lightweightParts;
    }

    function patchTouchesPhoneState(patch) {
        if (!patch) {
            return false;
        }

        const patchFields = Object.keys(patch).filter((key) => key !== "frameNumber");
        if (patchFields.includes("siteState")) {
            return true;
        }

        if (!patch.siteStatePatch) {
            return false;
        }

        return Object.prototype.hasOwnProperty.call(patch.siteStatePatch, "phonePanel") ||
            Object.prototype.hasOwnProperty.call(patch.siteStatePatch, "phoneListings");
    }

    function handleIncomingState(state, forceRender, patch) {
        try {
            const normalizedState = normalizeState(state);
            viewerCompatibilityWarning = getVisualSmokeCompatibilityWarning(normalizedState);
            rebuildInventoryCache(normalizedState);
            syncPlacementFailureToast(normalizedState);
            processRecentOneShotCues(normalizedState);
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
                if (lightweightPatchParts.placementPreview) {
                    if (isPlacementModeActive(normalizedState)) {
                        closeTileContextMenu();
                    }
                    updateSitePlacementPreviewVisual(normalizedState);
                }
                if (lightweightPatchParts.placementFailure) {
                    renderPlacementFailureToast();
                }
                if (lightweightPatchParts.protectionOverlay) {
                    if (isProtectionOverlayActive(normalizedState)) {
                        closeTileContextMenu();
                    }
                    updateSiteProtectionOverlayVisual(normalizedState);
                }
                if (lightweightPatchParts.siteAction) {
                    updateSiteExcavationMarksVisual(getSiteBootstrap(normalizedState), getSiteAction(normalizedState));
                    if (tileContextMenuState) {
                        renderTileContextMenu();
                    }
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
                    lightweightPatchParts.placementPreview ||
                    lightweightPatchParts.placementFailure ||
                    lightweightPatchParts.protectionOverlay ||
                    lightweightPatchParts.modifiers ||
                    lightweightPatchParts.weather ||
                    previousActionKind !== nextActionKind) {
                    renderSiteHudChrome(normalizedState);
                }
                if (lightweightPatchParts.hud) {
                    if (getTechTreeSetup(normalizedState) || getSiteProtectionSelectorSetup(normalizedState)) {
                        renderSiteOverlay(normalizedState);
                    } else {
                        renderSiteInventoryPanel(
                            normalizedState,
                            getInventorySlotsByKind(normalizedState, "WORKER_PACK"));
                        renderStoragePanel(normalizedState, findOpenedInventoryContainer(normalizedState));
                    }
                    if (phonePanelOpen && !isPhoneInteractionLocked()) {
                        renderPhonePanel(normalizedState);
                    }
                }
                renderActionProgressBar(normalizedState);
                if (lightweightPatchParts.audioCues) {
                    updateAudioMix(normalizedState);
                }
                return;
            }

            const forcePhonePanelRender = patchTouchesPhoneState(patch);
            if (forceRender || normalizedState.appState === "SITE_ACTIVE") {
                if (normalizedState.appState === "SITE_ACTIVE") {
                    latestPresentationSignature = "";
                } else {
                    latestPresentationSignature = buildPresentationSignature(normalizedState);
                }
                renderState(normalizedState, {
                    forcePhonePanelRender: forcePhonePanelRender
                });
            } else {
                const presentationSignature = buildPresentationSignature(normalizedState);
                if (presentationSignature !== latestPresentationSignature) {
                    latestPresentationSignature = presentationSignature;
                    renderState(normalizedState, {
                        forcePhonePanelRender: forcePhonePanelRender
                    });
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
            updateAudioMix(normalizedState);
        } catch (error) {
            reportViewerError("handleIncomingState", error);
            throw error;
        }
    }

    function connectStateStream() {
        if (stateStream) {
            stateStream.close();
        }

        stateStream = new EventSource("/events");
        stateStream.addEventListener("full-state", function (event) {
            try {
                const state = normalizeState(JSON.parse(event.data));
                handleIncomingState(state, true);
            } catch (error) {
                reportViewerError("full-state", error);
            }
        });
        stateStream.addEventListener("state-patch", function (event) {
            try {
                const patch = JSON.parse(event.data);
                const state = mergeStatePatch(patch);
                handleIncomingState(state, false, patch);
            } catch (error) {
                reportViewerError("state-patch", error);
            }
        });
        stateStream.addEventListener("frame-perf", function (event) {
            try {
                recordFramePerfSample(normalizeFramePerfSample(JSON.parse(event.data)));
            } catch (error) {
                reportViewerError("frame-perf", error);
            }
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
        if (!latestState || latestState.appState !== "SITE_ACTIVE") {
            return;
        }

        const siteControlState = computeSiteControlState();
        postJson("/site-control", siteControlState).catch(() => {});
    }

    window.addEventListener("keydown", function (event) {
        if ((event.code === "AltLeft" || event.code === "AltRight") &&
            !event.repeat &&
            latestState &&
            latestState.appState === "SITE_ACTIVE") {
            const protectionSelectorSetup = getSiteProtectionSelectorSetup(latestState);
            const request =
                protectionSelectorSetup || isProtectionOverlayActive(latestState)
                    ? postCloseSiteProtectionUi()
                    : postOpenSiteProtectionSelector();
            request.catch(() => {
                statusChip.textContent =
                    protectionSelectorSetup || isProtectionOverlayActive(latestState)
                        ? "Failed to close protection selector."
                        : "Failed to open protection selector.";
            });
            event.preventDefault();
            return;
        }
        if (event.code === "KeyF" && !event.repeat && latestState && latestState.appState === "SITE_ACTIVE") {
            const phoneIsOpen = isPhonePanelOpen(latestState);
            const phonePanelState = getPhonePanelState(latestState);
            const activeSection =
                phonePanelState && typeof phonePanelState.activeSection === "string"
                    ? phonePanelState.activeSection
                    : "HOME";
            const request = phoneIsOpen
                ? postClosePhonePanel()
                : postPhonePanelSection(activeSection);
            request.catch(() => {
                statusChip.textContent = phoneIsOpen
                    ? "Failed to close phone panel."
                    : "Failed to open phone panel.";
            });
            event.preventDefault();
            return;
        }
        if (event.code === "KeyB" && !event.repeat && latestState && latestState.appState === "SITE_ACTIVE") {
            const packIsOpen = isWorkerPackPanelOpen(latestState);
            postWorkerPackPanelEvent(packIsOpen ? "CLOSE" : "OPEN_SNAPSHOT").catch(() => {
                statusChip.textContent = packIsOpen
                    ? "Failed to close player pack."
                    : "Failed to open player pack.";
            });
            event.preventDefault();
            return;
        }
        if (event.code === "Escape" && latestState && latestState.appState === "SITE_ACTIVE") {
            closeTrackedSitePanelLayer(latestState).then((handledPanelClose) => {
                if (handledPanelClose) {
                    return;
                }
                if (isProtectionOverlayActive(latestState)) {
                    cancelProtectionOverlay();
                    return;
                }
                if (isPlacementModeActive(latestState)) {
                    cancelPlacementMode();
                    return;
                }
                cancelCurrentSiteAction().then((handledActionCancel) => {
                    if (handledActionCancel) {
                        return;
                    }
                    if (tileContextMenuState) {
                        closeTileContextMenu();
                    }
                });
            });
            event.preventDefault();
            return;
        }
        keys.add(event.code);
        updateMoveAxis();
    });

    window.addEventListener("keyup", function (event) {
        keys.delete(event.code);
        updateMoveAxis();
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

    renderer.domElement.addEventListener("pointermove", function (event) {
        if (!isSiteActiveView() || !siteSceneCache || !isPlacementModeActive(latestState)) {
            lastSentPlacementCursorSignature = "";
            return;
        }
        if (cameraOrbitDragPointerId != null || (pendingSiteOrbitGesture && pendingSiteOrbitGesture.dragging)) {
            return;
        }

        const tileData = pickSiteTileAtClientPosition(event.clientX, event.clientY);
        if (!tileData) {
            return;
        }

        sendPlacementCursorUpdate(tileData.tileX, tileData.tileY);
    });

    renderer.domElement.addEventListener("pointerup", function (event) {
        if (!isSiteActiveView() || !isPlacementModeActive(latestState) || event.button !== 0 || !siteSceneCache) {
            return;
        }

        const tileData = pickSiteTileAtClientPosition(event.clientX, event.clientY);
        if (tileData) {
            confirmPlacementModeAtTile(tileData);
        }
        event.preventDefault();
        event.stopPropagation();
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
            if (isProtectionOverlayActive(latestState)) {
                cancelProtectionOverlay();
            } else if (isPlacementModeActive(latestState)) {
                cancelPlacementMode();
            } else {
                const tileData = pickSiteTileAtClientPosition(releaseClientX, releaseClientY);
                if (!tileData) {
                    closeTileContextMenu();
                } else {
                    openTileContextMenu(tileData, releaseClientX, releaseClientY);
                }
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
        phonePointerInteractionActive = false;
        phoneWheelInteractionUntil = 0;
    });

    phoneLayer.addEventListener("mousedown", function (event) {
        if (!phonePanelOpen || event.button !== 0) {
            return;
        }

        phonePointerInteractionActive = true;
        event.stopPropagation();
    });

    phoneLayer.addEventListener("wheel", function (event) {
        if (!phonePanelOpen) {
            return;
        }

        phoneWheelInteractionUntil = performance.now() + 260;
        event.stopPropagation();
    }, { passive: true });

    phoneLayer.addEventListener("scroll", function (event) {
        if (!phonePanelOpen) {
            return;
        }

        const target = event.target;
        if (target === phoneScreenBody ||
            (target instanceof HTMLElement && target.hasAttribute("data-phone-scroll-key"))) {
            notePhoneScrollInteraction();
        }
    }, { passive: true, capture: true });

    document.addEventListener("mouseup", function () {
        if (!phonePointerInteractionActive) {
            return;
        }

        phonePointerInteractionActive = false;
        schedulePhonePanelRefreshAfterInteraction();
    }, true);

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
    window.addEventListener("error", function (event) {
        reportViewerError("window.error", event.error || event.message || "Unknown window error");
    });
    window.addEventListener("unhandledrejection", function (event) {
        reportViewerError("window.unhandledrejection", event.reason || "Unhandled rejection");
    });

    if (audioToggle) {
        audioToggle.addEventListener("click", function () {
            audioManager.enabled = !audioManager.enabled;
            if (!audioManager.enabled && audioManager.masterGain) {
                setAudioParamTarget(audioManager.masterGain.gain, 0.0, 0.05);
                updateAudioToggleButton();
                return;
            }

            resumeAudioGraphIfNeeded().then(function () {
                updateAudioMix(latestState);
            });
        });
    }

    document.addEventListener("pointerdown", function () {
        if (!audioManager.enabled) {
            return;
        }
        if (!audioManager.context || audioManager.context.state !== "running") {
            resumeAudioGraphIfNeeded().then(function () {
                updateAudioMix(latestState);
            });
        }
    }, { passive: true });

    updateAudioToggleButton();
    fitRenderer();
    storagePanelClose.addEventListener("click", function () {
        closeOpenedInventoryContainer();
    });
    animate();
    connectStateStream();
})();
