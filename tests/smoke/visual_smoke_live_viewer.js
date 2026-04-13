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
    const contextActions = document.getElementById("context-actions");

    let latestState = null;
    let stateStream = null;
    let inputSendInFlight = false;
    let mapPickables = [];
    let latestPresentationSignature = "";
    let currentSceneKind = "";
    let siteSceneCache = null;
    let inputDirty = true;
    let animationTimeSeconds = 0;
    let rendererWidth = 0;
    let rendererHeight = 0;

    const inputState = {
        moveX: 0,
        moveY: 0,
        cursorWorldX: 0,
        cursorWorldY: 0,
        buttonsDownMask: 0,
        buttonsPressedMask: 0,
        buttonsReleasedMask: 0
    };

    const keys = new Set();
    const raycaster = new THREE_NS.Raycaster();
    const pointer = new THREE_NS.Vector2();
    const animationClock = new THREE_NS.Clock();

    const renderer = new THREE_NS.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.outputColorSpace = THREE_NS.SRGBColorSpace;
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

    function updateMoveAxis() {
        const x = (keys.has("KeyD") || keys.has("ArrowRight") ? 1 : 0) - (keys.has("KeyA") || keys.has("ArrowLeft") ? 1 : 0);
        const y = (keys.has("KeyW") || keys.has("ArrowUp") ? 1 : 0) - (keys.has("KeyS") || keys.has("ArrowDown") ? 1 : 0);
        inputState.moveX = x;
        inputState.moveY = y;
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
        const hud = state.hud;
        const weather = siteState ? siteState.weather : null;

        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Site Active";
        selectionText.textContent = siteBootstrap
            ? "Site " + siteBootstrap.siteId + " is live. Use WASD to move through the desert staging ground."
            : "Site bootstrap is loading.";
        contextActions.innerHTML = "";

        const hydration = hud ? Math.round(hud.playerHydration) : 0;
        const energy = hud ? Math.round(hud.playerEnergy) : 0;
        const completion = hud ? Math.round((hud.siteCompletionNormalized || 0) * 100) : 0;
        const eventPhase = weather ? weather.eventPhase : "NONE";
        statusChip.textContent =
            "Site Live\nHydration " + hydration +
            "\nEnergy " + energy +
            "\nCompletion " + completion + "%" +
            "\nEvent " + eventPhase;
    }

    function updateOverlay(state) {
        stageFrame.classList.toggle("main-menu-mode", state.appState === "MAIN_MENU");
        stageFrame.classList.toggle("regional-map-mode", state.appState === "REGIONAL_MAP");
        hudEyebrow.textContent = "App State";
        hudTitle.textContent = state.appState || "NONE";

        switch (state.appState) {
        case "MAIN_MENU":
            hudSubtitle.textContent = "Austere, painterly, and severe: the campaign opens under hostile conditions.";
            renderMenuOverlay(state);
            statusChip.textContent = "Prototype Build\nVisual Smoke";
            break;
        case "REGIONAL_MAP":
            hudSubtitle.textContent = "Review the campaign survey board and choose the next deployment route.";
            renderRegionalMapOverlay(state);
            statusChip.textContent =
                "Campaign Survey\nFrame " + state.frameNumber +
                "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            break;
        case "SITE_ACTIVE":
            hudSubtitle.textContent = "Field movement is now live. This slice proves the site scene, camera, and worker control loop.";
            renderSiteOverlay(state);
            break;
        default:
            hudSubtitle.textContent = "The current adapter only styles the core early flow for now.";
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

    function updateHumanoidWorkerAnimation(cache, deltaSeconds, elapsed, movementSpeed, distanceToTarget) {
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

    function rebuildStaticSiteScene(siteBootstrap, offsetX, offsetZ, width, height) {
        clearWorld();
        currentSceneKind = "SITE_ACTIVE";
        scene.background = new THREE_NS.Color(0xe7d3b0);

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
            const tileColor = hasPlant
                ? 0x97a66d
                : burial > 0.45
                    ? 0xdfc090
                    : 0xc9a776;

            const tileMesh = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(0.94, tileHeight, 0.94),
                new THREE_NS.MeshStandardMaterial({ color: tileColor, roughness: 0.9, metalness: 0.02 })
            );
            tileMesh.position.set(tile.x - offsetX, tileHeight * 0.5, tile.y - offsetZ);
            worldGroup.add(tileMesh);

            if (hasPlant) {
                const reed = new THREE_NS.Mesh(
                    new THREE_NS.CylinderGeometry(0.04, 0.08, 0.25 + plantDensity * 0.45, 6),
                    new THREE_NS.MeshStandardMaterial({ color: 0x657c4f, roughness: 0.82, metalness: 0.01 })
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
            bootstrapSignature: buildSiteBootstrapSignature(siteBootstrap),
            offsetX: offsetX,
            offsetZ: offsetZ,
            width: width,
            height: height,
            workerGroup: workerGroup,
            workerRig: workerVisual.rig,
            workerInitialized: false,
            workerAnimPhase: 0,
            workerVisualSpeed: 0,
            workerTargetX: 0,
            workerTargetZ: 0,
            workerTargetYaw: 0,
            cameraTargetX: 0,
            cameraTargetZ: 0
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
        const bootstrapSignature = buildSiteBootstrapSignature(siteBootstrap);

        const needsStaticRebuild =
            currentSceneKind !== "SITE_ACTIVE" ||
            !siteSceneCache ||
            siteSceneCache.siteId !== siteBootstrap.siteId ||
            siteSceneCache.width !== width ||
            siteSceneCache.height !== height ||
            siteSceneCache.bootstrapSignature !== bootstrapSignature;

        if (needsStaticRebuild) {
            rebuildStaticSiteScene(siteBootstrap, offsetX, offsetZ, width, height);
        }

        if (!siteSceneCache) {
            return;
        }

        let cameraTargetX = 0;
        let cameraTargetZ = 0;
        if (siteState && siteState.worker) {
            cameraTargetX = siteState.worker.tileX - siteSceneCache.offsetX;
            cameraTargetZ = siteState.worker.tileY - siteSceneCache.offsetZ;
            siteSceneCache.workerTargetX = cameraTargetX;
            siteSceneCache.workerTargetZ = cameraTargetZ;
            siteSceneCache.workerTargetYaw = (siteState.worker.facingDegrees || 0) * Math.PI / 180.0;

            if (!siteSceneCache.workerInitialized) {
                siteSceneCache.workerGroup.position.set(cameraTargetX, 0.0, cameraTargetZ);
                siteSceneCache.workerGroup.rotation.y = siteSceneCache.workerTargetYaw;
                siteSceneCache.workerInitialized = true;
            }
        }

        siteSceneCache.cameraTargetX = cameraTargetX;
        siteSceneCache.cameraTargetZ = cameraTargetZ;

        if (needsStaticRebuild) {
            camera.position.set(
                cameraTargetX + 4.6,
                Math.max(width, height) * 0.72 + 3.0,
                cameraTargetZ + 5.2
            );
            camera.lookAt(cameraTargetX, 0.4, cameraTargetZ);
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
            const workerBlend = blendFactorFromRate(18.0, deltaSeconds);
            const rotationBlend = blendFactorFromRate(16.0, deltaSeconds);
            const cameraBlend = blendFactorFromRate(10.0, deltaSeconds);
            const previousWorkerX = workerGroup.position.x;
            const previousWorkerZ = workerGroup.position.z;
            const distanceToTargetBeforeMove = Math.hypot(
                siteSceneCache.workerTargetX - previousWorkerX,
                siteSceneCache.workerTargetZ - previousWorkerZ
            );
            workerGroup.position.x += (siteSceneCache.workerTargetX - workerGroup.position.x) * workerBlend;
            workerGroup.position.z += (siteSceneCache.workerTargetZ - workerGroup.position.z) * workerBlend;

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
            const targetPullSpeed = deltaSeconds > 0.0
                ? Math.min(distanceToTargetBeforeMove / deltaSeconds, 3.8)
                : 0.0;
            const desiredWorkerSpeed = Math.max(visualSpeed, distanceToTarget > 0.006 ? targetPullSpeed : 0.0);
            const speedBlend = blendFactorFromRate(12.0, deltaSeconds);
            siteSceneCache.workerVisualSpeed += (desiredWorkerSpeed - siteSceneCache.workerVisualSpeed) * speedBlend;
            updateHumanoidWorkerAnimation(
                siteSceneCache,
                deltaSeconds,
                elapsed,
                siteSceneCache.workerVisualSpeed,
                distanceToTarget
            );

            const desiredCameraX = siteSceneCache.cameraTargetX + 4.6;
            const desiredCameraY = Math.max(siteSceneCache.width, siteSceneCache.height) * 0.72 + 3.0;
            const desiredCameraZ = siteSceneCache.cameraTargetZ + 5.2;
            camera.position.x += (desiredCameraX - camera.position.x) * cameraBlend;
            camera.position.y += (desiredCameraY - camera.position.y) * cameraBlend;
            camera.position.z += (desiredCameraZ - camera.position.z) * cameraBlend;
            camera.lookAt(workerGroup.position.x, 0.4, workerGroup.position.z);
        }

        fitRenderer();
        renderer.render(scene, camera);
    }

    function normalizeState(state) {
        if (!state.uiSetups) {
            state.uiSetups = [];
        }
        if (!state.regionalMap) {
            state.regionalMap = { sites: [], links: [] };
        }
        return state;
    }

    function mergeStatePatch(patch) {
        return normalizeState(Object.assign({}, latestState || {}, patch));
    }

    function handleIncomingState(state, forceRender) {
        const normalizedState = normalizeState(state);
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

        if (normalizedState.appState !== "MAIN_MENU") {
            statusChip.textContent =
                "Connected\nFrame " + normalizedState.frameNumber +
                "\nSelected Site: " + (normalizedState.selectedSiteId == null ? "none" : normalizedState.selectedSiteId);
        }
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
            handleIncomingState(state, false);
        });
        stateStream.onerror = function () {
            statusChip.textContent = "Waiting for host...";
        };
    }

    function sendInputState() {
        if (inputSendInFlight || !inputDirty) {
            return;
        }

        inputSendInFlight = true;
        postJson("/input", {
            moveX: inputState.moveX,
            moveY: inputState.moveY,
            cursorWorldX: inputState.cursorWorldX,
            cursorWorldY: inputState.cursorWorldY,
            buttonsDownMask: inputState.buttonsDownMask,
            buttonsPressedMask: inputState.buttonsPressedMask,
            buttonsReleasedMask: inputState.buttonsReleasedMask
        })
            .then(() => {
                inputState.buttonsPressedMask = 0;
                inputState.buttonsReleasedMask = 0;
                inputDirty = false;
            })
            .catch(() => {})
            .finally(() => {
                inputSendInFlight = false;
            });
    }

    function updateCursorWorld(event) {
        const rect = gameView.getBoundingClientRect();
        const x = ((event.clientX - rect.left) / Math.max(rect.width, 1)) * 2 - 1;
        const y = ((event.clientY - rect.top) / Math.max(rect.height, 1)) * 2 - 1;
        inputState.cursorWorldX = x;
        inputState.cursorWorldY = -y;
    }

    window.addEventListener("keydown", function (event) {
        keys.add(event.code);
        updateMoveAxis();
        inputDirty = true;
        sendInputState();
    });

    window.addEventListener("keyup", function (event) {
        keys.delete(event.code);
        updateMoveAxis();
        inputDirty = true;
        sendInputState();
    });

    gameView.addEventListener("pointermove", function (event) {
        updateCursorWorld(event);
        inputDirty = true;
    });

    gameView.addEventListener("pointerdown", function (event) {
        updateCursorWorld(event);
        inputState.buttonsDownMask |= 1;
        inputState.buttonsPressedMask |= 1;
        inputDirty = true;
        sendInputState();
    });

    window.addEventListener("pointerup", function () {
        if ((inputState.buttonsDownMask & 1) !== 0) {
            inputState.buttonsReleasedMask |= 1;
        }
        inputState.buttonsDownMask &= ~1;
        inputDirty = true;
        sendInputState();
    });

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
    window.setInterval(sendInputState, 16);
})();
