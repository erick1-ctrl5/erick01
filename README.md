# RayForcé

Un motor de renderizado 3D por **raytracing** escrito desde cero en C, sin una
sola dependencia: sin librerías gráficas, sin OpenGL, solo matemáticas y
pthreads. Cada píxel es un rayo lanzado contra la escena y seguido a través de
sus rebotos.

## La escena

- 4 esferas: una **cromada** (reflexividad 0.92), una **roja difusa**, una
  **ámbar emisiva** (luz propia) y una **verde espejo** flotando.
- **Suelo de ajedrez** infinito con desvanecimiento atmosférico y reflexión.
- Cielo degradado tipo atardecer (horizonte cálido, cenit oscuro).
- 2 luces puntuales con **sombras** proyectadas, atenuación por distancia,
  sombreado **Blinn-Phong** (difusa + especular).
- Reflexiones **recursivas** hasta 5 rebotes.

## Características del motor

- Escena definida 100% en datos: añadir esferas o luces = añadir una línea.
- Antialiasing por supersampling configurable (por defecto 2×2 por píxel).
- Corrección gamma (1/2.2) y tonemapping por saturación.
- **Multithreading**: la imagen se divide en franjas entre 8 hilos.
- Compila en un comando con `gcc`, sin configurar nada.

## Uso

```bash
gcc -O3 -Wall -pthread raytracer.c -o ray -lm
./ray            > escena.ppm    # vista frontal
./ray 35         > angel35.ppm   # órbita: ángulo en grados
./ray 215        > trasera.ppm   # vista desde atrás
```

La salida es una imagen **PPM (P6)** estándar, convertible con GIMP, ImageMagick
(`convert escena.ppm escena.png`) o cualquier visor.

## Bonus: render a ASCII en vivo

`ppm2asc.py` convierte cualquier PPM a arte ASCII cuantizado:

```bash
python3 ppm2asc.py escena.ppm 100   # 100 columnas
```

## Órbita completa (animación)

```bash
for a in $(seq 0 10 360); do ./ray $a > orbit/$(printf %03d $a).ppm; done
```

## Archivos

| Archivo        | Descripción                                  |
|----------------|----------------------------------------------|
| `raytracer.c`  | El motor completo (~230 líneas de C)         |
| `ppm2asc.py`   | Conversor PPM → ASCII con rampa cuantizada   |
| `frame_*.ppm`  | Fotogramas de la órbita                      |
| `hero.ppm`     | Render de 960×540 presenting la escena       |

## Ver sin instalar nada

- `orbit.gif` — animación de la órbita completa de la cámara alrededor de la escena
- `hero.png` — vista frontal renderizada a 960×540
- `vista215.png` — vista trasera (ángulo 215°)

GitHub los muestra directamente en el navegador.
