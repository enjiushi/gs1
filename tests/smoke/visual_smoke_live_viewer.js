import * as THREE_NS from "https://unpkg.com/three@0.165.0/build/three.module.js";

(function () {
    const gameView = document.getElementById("game-view");
    const hudEyebrow = document.getElementById("hud-eyebrow");
    const hudTitle = document.getElementById("hud-title");
    const hudSubtitle = document.getElementById("hud-subtitle");
    const statusChip = document.getElementById("status-chip");
    const menuPanel = document.getElementById("menu-panel");
    const menuEyebrow = document.getElementById("menu-eyebrow");
    const menuTitle = document.getElementById("menu-title");
    const menuCopy = document.getElementById("menu-copy");
    const menuActions = document.getElementById("menu-actions");
    const selectionEyebrow = document.getElementById("selection-eyebrow");
    const selectionText = document.getElementById("selection-text");
    const contextActions = document.getElementById("context-actions");

    let latestState = null;
    let stateFetchInFlight = false;
    let inputSendInFlight = false;
    let mapPickables = [];
    let latestPresentationSignature = "";

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

    function getLabelElements(setup) {
        if (!setup) {
            return [];
        }
        return setup.elements.filter((element) => !element.action || element.action.type === "NONE");
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
        menuEyebrow.textContent = "Placeholder Main Menu";
        menuTitle.textContent = "GS1";
        menuCopy.textContent = "Start a new campaign to enter the regional map. This UI is intentionally simple, but it is now driven by the live adapter state.";
        menuActions.innerHTML = "";

        actionElements.forEach((element, index) => {
            menuActions.appendChild(
                makeButton(
                    element.text || element.action.type,
                    function () {
                        postJson("/ui-action", element.action).catch(() => {
                            statusChip.textContent = "Failed to send UI action.";
                        });
                    },
                    index > 0,
                    (element.flags & 2) !== 0
                )
            );
        });

        selectionEyebrow.textContent = "Flow";
        selectionText.textContent = "Main menu is active. Click Start New Campaign to transition into the regional map.";
        contextActions.innerHTML = "";
    }

    function renderRegionalMapOverlay(state) {
        const selectionSetup = getSetup(state, "REGIONAL_MAP_SELECTION");
        const labels = getLabelElements(selectionSetup);
        const actions = getActionElements(selectionSetup);

        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Regional Map";

        if (labels.length > 0) {
            selectionText.textContent = labels.map((entry) => entry.text).join(" | ");
        } else if (state.selectedSiteId != null) {
            selectionText.textContent = "Selected Site " + state.selectedSiteId;
        } else {
            selectionText.textContent = "Choose a deployment site.";
        }

        contextActions.innerHTML = "";
        actions.forEach((element) => {
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
    }

    function renderFallbackOverlay(state) {
        menuPanel.hidden = true;
        selectionEyebrow.textContent = "Current View";
        selectionText.textContent =
            state.siteSnapshot
                ? "Site " + state.siteSnapshot.siteId + " placeholder view."
                : "Waiting for the next presentation state.";
        contextActions.innerHTML = "";
    }

    function updateOverlay(state) {
        hudEyebrow.textContent = "App State";
        hudTitle.textContent = state.appState || "NONE";

        switch (state.appState) {
        case "MAIN_MENU":
            hudSubtitle.textContent = "A live placeholder main menu presented through the visual adapter.";
            renderMenuOverlay(state);
            break;
        case "REGIONAL_MAP":
            hudSubtitle.textContent = "Select a deployment site to continue the current flow.";
            renderRegionalMapOverlay(state);
            break;
        default:
            hudSubtitle.textContent = "The current adapter only styles the core early flow for now.";
            renderFallbackOverlay(state);
            break;
        }

        statusChip.textContent =
            "Connected\nFrame " + state.frameNumber +
            "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
    }

    function renderMainMenuScene() {
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

    function renderRegionalMapScene(state) {
        scene.background = new THREE_NS.Color(0xefe2ca);

        const floor = new THREE_NS.Mesh(
            new THREE_NS.PlaneGeometry(32, 18),
            new THREE_NS.MeshStandardMaterial({ color: 0xf2e6d4, roughness: 0.95, metalness: 0.02 })
        );
        floor.rotation.x = -Math.PI / 2;
        floor.position.y = -0.03;
        worldGroup.add(floor);
        worldGroup.add(new THREE_NS.GridHelper(30, 20, 0xd0bf9f, 0xe7dbc8));

        const positions = new Map();
        const scale = 0.03;

        state.regionalMap.sites.forEach((site) => {
            positions.set(site.siteId, { x: site.mapX * scale, z: -site.mapY * scale });
        });

        state.regionalMap.links.forEach((link) => {
            const from = positions.get(link.fromSiteId);
            const to = positions.get(link.toSiteId);
            if (!from || !to) {
                return;
            }

            const geometry = new THREE_NS.BufferGeometry().setFromPoints([
                new THREE_NS.Vector3(from.x, 0.09, from.z),
                new THREE_NS.Vector3(to.x, 0.09, to.z)
            ]);
            const line = new THREE_NS.Line(
                geometry,
                new THREE_NS.LineBasicMaterial({ color: 0x8b6f4f })
            );
            worldGroup.add(line);
        });

        state.regionalMap.sites.forEach((site, index) => {
            const position = positions.get(site.siteId);
            if (!position) {
                return;
            }

            const stateColor =
                site.siteState === "AVAILABLE" ? 0x88b173 :
                site.siteState === "COMPLETED" ? 0xcda261 :
                0x808995;

            const base = new THREE_NS.Mesh(
                new THREE_NS.CylinderGeometry(0.75, 0.98, 0.34, 28),
                new THREE_NS.MeshStandardMaterial({ color: stateColor, roughness: 0.78, metalness: 0.08 })
            );
            base.position.set(position.x, 0.18, position.z);
            base.userData = { siteId: site.siteId, siteState: site.siteState, pulseOffset: index * 0.8 };
            worldGroup.add(base);

            if (site.siteState === "AVAILABLE") {
                mapPickables.push(base);

                const pulseRing = new THREE_NS.Mesh(
                    new THREE_NS.TorusGeometry(1.05, 0.05, 12, 48),
                    new THREE_NS.MeshStandardMaterial({ color: 0x6e9a65, emissive: 0x50704a, emissiveIntensity: 0.22 })
                );
                pulseRing.rotation.x = Math.PI / 2;
                pulseRing.position.set(position.x, 0.24, position.z);
                pulseRing.userData = { pulseOffset: index * 0.8, pulse: true };
                worldGroup.add(pulseRing);
            }

            if ((site.flags & 1) !== 0) {
                const selectedHalo = new THREE_NS.Mesh(
                    new THREE_NS.TorusGeometry(1.32, 0.09, 14, 64),
                    new THREE_NS.MeshStandardMaterial({ color: 0x2f3f4f, emissive: 0x2f3f4f, emissiveIntensity: 0.18 })
                );
                selectedHalo.rotation.x = Math.PI / 2;
                selectedHalo.position.set(position.x, 0.34, position.z);
                selectedHalo.userData = { spinZ: 0.46 };
                worldGroup.add(selectedHalo);
            }
        });

        if (state.regionalMap.sites.length > 0) {
            const xs = state.regionalMap.sites.map((site) => positions.get(site.siteId).x);
            const zs = state.regionalMap.sites.map((site) => positions.get(site.siteId).z);
            const centerX = (Math.min.apply(null, xs) + Math.max.apply(null, xs)) * 0.5;
            const centerZ = (Math.min.apply(null, zs) + Math.max.apply(null, zs)) * 0.5;
            camera.position.set(centerX + 5.2, 8.4, centerZ + 7.6);
            camera.lookAt(centerX, 0, centerZ);
        } else {
            camera.position.set(5, 8, 7);
            camera.lookAt(0, 0, 0);
        }
    }

    function renderSitePlaceholderScene(state) {
        scene.background = new THREE_NS.Color(0xeee2cf);

        const floor = new THREE_NS.Mesh(
            new THREE_NS.PlaneGeometry(18, 18),
            new THREE_NS.MeshStandardMaterial({ color: 0xf0e4d2, roughness: 0.95 })
        );
        floor.rotation.x = -Math.PI / 2;
        floor.position.y = -0.03;
        worldGroup.add(floor);

        if (state.siteSnapshot) {
            const snapshot = state.siteSnapshot;
            const width = Math.max(snapshot.width, 1);
            const height = Math.max(snapshot.height, 1);
            const offsetX = (width - 1) * 0.5;
            const offsetZ = (height - 1) * 0.5;

            snapshot.tiles.forEach((tile) => {
                const tileHeight = 0.08 + Math.max(0, Math.min(tile.plantDensity || 0, 1)) * 0.3;
                const tileMesh = new THREE_NS.Mesh(
                    new THREE_NS.BoxGeometry(0.94, tileHeight, 0.94),
                    new THREE_NS.MeshStandardMaterial({ color: 0xcaab7b, roughness: 0.88 })
                );
                tileMesh.position.set(tile.x - offsetX, tileHeight * 0.5, tile.y - offsetZ);
                worldGroup.add(tileMesh);
            });

            if (snapshot.worker) {
                const workerMesh = new THREE_NS.Mesh(
                    new THREE_NS.SphereGeometry(0.32, 18, 18),
                    new THREE_NS.MeshStandardMaterial({ color: 0x2f7f91 })
                );
                workerMesh.position.set(snapshot.worker.tileX - offsetX, 0.42, snapshot.worker.tileY - offsetZ);
                worldGroup.add(workerMesh);
            }

            camera.position.set(width * 0.95, Math.max(width, height), height * 1.25);
            camera.lookAt(0, 0, 0);
        } else {
            const placeholder = new THREE_NS.Mesh(
                new THREE_NS.BoxGeometry(2.4, 1.2, 2.4),
                new THREE_NS.MeshStandardMaterial({ color: 0x9f7c53, roughness: 0.82 })
            );
            placeholder.position.set(0, 0.6, 0);
            placeholder.userData = { spinY: 0.12 };
            worldGroup.add(placeholder);
            camera.position.set(4.5, 5.8, 7.2);
            camera.lookAt(0, 0.5, 0);
        }
    }

    function rebuildWorld(state) {
        clearWorld();

        switch (state.appState) {
        case "MAIN_MENU":
            renderMainMenuScene();
            break;
        case "REGIONAL_MAP":
            renderRegionalMapScene(state);
            break;
        default:
            renderSitePlaceholderScene(state);
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
            siteSnapshot: state.siteSnapshot,
            hud: state.hud,
            siteResult: state.siteResult
        });
    }

    function animate() {
        requestAnimationFrame(animate);

        const elapsed = animationClock.getElapsedTime();
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

        fitRenderer();
        renderer.render(scene, camera);
    }

    function fetchState() {
        if (stateFetchInFlight) {
            return;
        }

        stateFetchInFlight = true;
        fetch("/state", { cache: "no-store" })
            .then((response) => response.json())
            .then((state) => {
                const presentationSignature = buildPresentationSignature(state);
                if (presentationSignature !== latestPresentationSignature) {
                    latestPresentationSignature = presentationSignature;
                    renderState(state);
                } else {
                    latestState = state;
                }
                statusChip.textContent =
                    "Connected\nFrame " + state.frameNumber +
                    "\nSelected Site: " + (state.selectedSiteId == null ? "none" : state.selectedSiteId);
            })
            .catch(() => {
                statusChip.textContent = "Waiting for host...";
            })
            .finally(() => {
                stateFetchInFlight = false;
            });
    }

    function sendInputState() {
        if (inputSendInFlight) {
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
            .catch(() => {})
            .finally(() => {
                inputState.buttonsPressedMask = 0;
                inputState.buttonsReleasedMask = 0;
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
    });

    window.addEventListener("keyup", function (event) {
        keys.delete(event.code);
        updateMoveAxis();
    });

    gameView.addEventListener("pointermove", function (event) {
        updateCursorWorld(event);
    });

    gameView.addEventListener("pointerdown", function (event) {
        updateCursorWorld(event);
        inputState.buttonsDownMask |= 1;
        inputState.buttonsPressedMask |= 1;
    });

    window.addEventListener("pointerup", function () {
        if ((inputState.buttonsDownMask & 1) !== 0) {
            inputState.buttonsReleasedMask |= 1;
        }
        inputState.buttonsDownMask &= ~1;
    });

    renderer.domElement.addEventListener("click", function (event) {
        if (!latestState || latestState.appState !== "REGIONAL_MAP" || mapPickables.length === 0) {
            return;
        }

        const rect = renderer.domElement.getBoundingClientRect();
        pointer.x = ((event.clientX - rect.left) / Math.max(rect.width, 1)) * 2 - 1;
        pointer.y = -(((event.clientY - rect.top) / Math.max(rect.height, 1)) * 2 - 1);
        raycaster.setFromCamera(pointer, camera);

        const hits = raycaster.intersectObjects(mapPickables, false);
        if (hits.length === 0) {
            return;
        }

        postJson("/ui-action", {
            type: "SELECT_DEPLOYMENT_SITE",
            targetId: hits[0].object.userData.siteId,
            arg0: 0,
            arg1: 0
        }).catch(() => {
            statusChip.textContent = "Failed to select site.";
        });
    });

    window.addEventListener("resize", fitRenderer);

    fitRenderer();
    animate();
    fetchState();
    window.setInterval(fetchState, 100);
    window.setInterval(sendInputState, 66);
})();
