import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import type { Frame } from "./simulator";

// Boost the rendered light while preserving the brightness ratios in the frame.
const LIGHT_DISPLAY_GAIN = 4;

export class LampScene {
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera = new THREE.PerspectiveCamera(34, 1, 0.1, 40);
  private controls: OrbitControls;
  private shell: THREE.Mesh;
  private pixels: THREE.Mesh[] = [];
  private colors = Array.from({ length: 16 }, () => new THREE.Vector3());
  private halo: THREE.Mesh;
  private selection: THREE.Mesh;
  private selected = 0;
  constructor(canvas: HTMLCanvasElement) {
    this.renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
    });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.3;
    this.controls = new OrbitControls(this.camera, canvas);
    this.controls.enableDamping = true;
    this.controls.enablePan = false;
    this.controls.minDistance = 4;
    this.controls.maxDistance = 12;
    this.controls.maxPolarAngle = Math.PI / 2 - 0.04;
    this.resetView();
    this.scene.add(new THREE.HemisphereLight(0xcad7e6, 0x323940, 2));
    const key = new THREE.DirectionalLight(0xffffff, 3);
    key.position.set(-3, 6, 4);
    this.scene.add(key);
    const rim = new THREE.DirectionalLight(0x91afc6, 2);
    rim.position.set(3, 3, -3);
    this.scene.add(rim);
    const baseMaterial = new THREE.MeshStandardMaterial({
      color: 0x72787b,
      metalness: 0.3,
      roughness: 0.56,
    });
    const base = new THREE.Mesh(
      new THREE.CylinderGeometry(1.01, 0.97, 0.22, 96),
      baseMaterial,
    );
    base.position.y = 0.14;
    this.scene.add(base);
    const foot = new THREE.Mesh(
      new THREE.CylinderGeometry(0.9, 0.9, 0.05, 96),
      new THREE.MeshStandardMaterial({ color: 0x24282a, roughness: 0.8 }),
    );
    foot.position.y = 0.025;
    this.scene.add(foot);
    const ring = new THREE.Mesh(
      new THREE.RingGeometry(0.77, 0.95, 96),
      new THREE.MeshStandardMaterial({
        color: 0xd5cead,
        roughness: 0.7,
        side: THREE.DoubleSide,
      }),
    );
    ring.rotation.x = -Math.PI / 2;
    ring.position.y = 0.253;
    this.scene.add(ring);
    const pcb = new THREE.Mesh(
      new THREE.BoxGeometry(0.35, 0.035, 0.68),
      new THREE.MeshStandardMaterial({ color: 0x166b9f, roughness: 0.8 }),
    );
    pcb.position.set(0, 0.3, 0);
    this.scene.add(pcb);
    const chip = new THREE.Mesh(
      new THREE.BoxGeometry(0.19, 0.025, 0.2),
      new THREE.MeshStandardMaterial({ color: 0x202427 }),
    );
    chip.position.set(0, 0.33, 0);
    this.scene.add(chip);
    for (let i = 0; i < 16; i++) {
      const angle = (i * Math.PI * 2) / 16;
      const casing = new THREE.Mesh(
        new THREE.BoxGeometry(0.13, 0.04, 0.13),
        new THREE.MeshStandardMaterial({ color: 0xe9e5d9, roughness: 0.65 }),
      );
      casing.position.set(Math.sin(angle) * 0.86, 0.28, Math.cos(angle) * 0.86);
      casing.rotation.y = angle;
      this.scene.add(casing);
      const pixel = new THREE.Mesh(
        new THREE.BoxGeometry(0.09, 0.008, 0.09),
        new THREE.MeshStandardMaterial({
          color: 0xffffff,
          emissive: 0xffffff,
          emissiveIntensity: 2,
          roughness: 0.4,
        }),
      );
      pixel.position.copy(casing.position);
      pixel.position.y += 0.024;
      pixel.rotation.y = angle;
      this.scene.add(pixel);
      this.pixels.push(pixel);
    }
    const curve = new THREE.SplineCurve([
      new THREE.Vector2(0.985, 0.26),
      new THREE.Vector2(1.0, 0.6),
      new THREE.Vector2(1.01, 1.05),
      new THREE.Vector2(0.95, 1.55),
      new THREE.Vector2(0.79, 2.0),
      new THREE.Vector2(0.48, 2.35),
      new THREE.Vector2(0.16, 2.51),
      new THREE.Vector2(0, 2.53),
    ]);
    const material = new THREE.ShaderMaterial({
      uniforms: { pixels: { value: this.colors } },
      vertexShader: `varying vec3 pos; varying vec3 norm; varying vec3 world;
        void main() { pos=position; norm=normalize(mat3(modelMatrix)*normal);
        world=(modelMatrix*vec4(position,1.)).xyz;
        gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.); }`,
      fragmentShader: `uniform vec3 pixels[16]; varying vec3 pos; varying vec3 norm; varying vec3 world;
        void main() {
          vec3 glow=vec3(0.); float weight=0.;
          for(int i=0;i<16;i++) {
            float a=float(i)*6.2831853/16.;
            vec3 source=vec3(sin(a)*.86,.28,cos(a)*.86);
            float w=1./(dot(pos-source,pos-source)+.19);
            glow+=pixels[i]*w; weight+=w;
          }
          glow/=weight;
          vec3 n=normalize(norm); vec3 eye=normalize(cameraPosition-world);
          float facing=max(0.,dot(n,eye));
          float light=.32+.36*max(0.,dot(n,normalize(vec3(-3.,5.,4.))));
          float edge=pow(1.-facing,2.);
          vec3 frost=vec3(.49,.54,.55)*light+vec3(.11)*edge;
          float falloff=1.9*exp(-max(0.,pos.y-.35)*.48);
          gl_FragColor=vec4(frost+glow*falloff,1.);
          #include <tonemapping_fragment>
          #include <colorspace_fragment>
        }`,
    });
    this.shell = new THREE.Mesh(
      new THREE.LatheGeometry(curve.getPoints(64), 96),
      material,
    );
    this.scene.add(this.shell);
    const textureCanvas = document.createElement("canvas");
    textureCanvas.width = textureCanvas.height = 128;
    const ctx = textureCanvas.getContext("2d")!;
    const gradient = ctx.createRadialGradient(64, 64, 5, 64, 64, 64);
    gradient.addColorStop(0, "rgba(255,255,255,0.6)");
    gradient.addColorStop(0.4, "rgba(255,255,255,0.3)");
    gradient.addColorStop(1, "rgba(255,255,255,0)");
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, 128, 128);
    this.halo = new THREE.Mesh(
      new THREE.PlaneGeometry(4.8, 4.8),
      new THREE.MeshBasicMaterial({
        map: new THREE.CanvasTexture(textureCanvas),
        transparent: true,
        opacity: 0.35,
        depthWrite: false,
        blending: THREE.AdditiveBlending,
      }),
    );
    this.halo.rotation.x = -Math.PI / 2;
    this.halo.position.y = 0.005;
    this.scene.add(this.halo);
    const grid = new THREE.GridHelper(20, 40, 0x364148, 0x303a41);
    grid.position.y = -0.01;
    grid.material.transparent = true;
    grid.material.opacity = 0.26;
    this.scene.add(grid);
    this.selection = new THREE.Mesh(
      new THREE.RingGeometry(0.1, 0.12, 32),
      new THREE.MeshBasicMaterial({
        color: 0xc7ef8c,
        side: THREE.DoubleSide,
        depthTest: false,
      }),
    );
    this.selection.rotation.x = -Math.PI / 2;
    this.selection.renderOrder = 2;
    this.selection.visible = false;
    this.scene.add(this.selection);
    let downX = 0,
      downY = 0;
    canvas.addEventListener("pointerdown", (e) => {
      downX = e.clientX;
      downY = e.clientY;
    });
    canvas.addEventListener("pointerup", (e) => {
      if (
        this.shell.visible ||
        Math.hypot(e.clientX - downX, e.clientY - downY) > 5
      )
        return;
      const bounds = canvas.getBoundingClientRect();
      const ray = new THREE.Raycaster();
      ray.setFromCamera(
        new THREE.Vector2(
          ((e.clientX - bounds.left) / bounds.width) * 2 - 1,
          (-(e.clientY - bounds.top) / bounds.height) * 2 + 1,
        ),
        this.camera,
      );
      const hit = ray.intersectObjects(this.pixels)[0];
      if (hit)
        canvas.dispatchEvent(
          new CustomEvent("pixel-select", {
            detail: this.pixels.indexOf(hit.object as THREE.Mesh),
          }),
        );
    });
    new ResizeObserver(() => {
      const { width, height } = canvas.getBoundingClientRect();
      this.camera.aspect = width / height;
      this.camera.updateProjectionMatrix();
      this.renderer.setSize(width, height, false);
    }).observe(canvas);
    canvas.dataset.ready = "true";
  }
  resetView() {
    this.camera.position.set(3.5, 2.9, 5.6);
    this.controls.target.set(0, 1.12, 0);
    this.controls.update();
  }
  diffuser(visible: boolean) {
    this.shell.visible = visible;
    this.selection.visible = !visible;
  }
  select(index: number) {
    this.selected = index;
  }
  update(frame: Frame) {
    const average = new THREE.Color(0, 0, 0);
    const gain = (frame.brightness / 255) * LIGHT_DISPLAY_GAIN;
    for (let i = 0; i < 16; i++) {
      // Retain relative brightness and TypicalLEDStrip correction under the
      // fixed display gain. RGB inspection uses the original frame bytes.
      const color = new THREE.Color().setRGB(
        (frame.rgb[i * 3] / 255) * gain,
        ((frame.rgb[i * 3 + 1] / 255) * gain * 176) / 255,
        ((frame.rgb[i * 3 + 2] / 255) * gain * 240) / 255,
        THREE.LinearSRGBColorSpace,
      );
      this.colors[i].set(color.r, color.g, color.b);
      average.add(color);
      const material = this.pixels[i].material as THREE.MeshStandardMaterial;
      material.color.copy(color);
      material.emissive.copy(color);
    }
    (this.halo.material as THREE.MeshBasicMaterial).color.copy(
      average.multiplyScalar(1 / 16),
    );
  }
  render() {
    this.selection.position.copy(this.pixels[this.selected].position);
    this.selection.position.y += 0.015;
    this.controls.update();
    this.renderer.render(this.scene, this.camera);
  }
}
