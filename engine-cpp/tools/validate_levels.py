#!/usr/bin/env python3
"""Validador de conectividad de los niveles (assets/data/*.json).

Herramienta de AUTORIA: no forma parte del binario del juego ni se compila
con el, se ejecuta a mano al generar o tocar un nivel.

    python engine-cpp/tools/validate_levels.py            # todos los niveles
    python engine-cpp/tools/validate_levels.py level_13   # uno suelto

Comprueba, por flood-fill desde el spawn del jugador, que la puerta y todos
los objetos recogibles son ALCANZABLES caminando -- que es justo lo que no se
ve leyendo el JSON y lo que un playtest tarda minutos en descubrir. Nacio de
un fallo real: el laberinto del nivel 7, escrito a ojo, dejaba al jugador en
una bolsa sellada por cuatro muros.

El modelo de colision replica EXACTAMENTE el del motor (Entity::TryMove +
CollisionMath::AABBIntersects), incluida la aproximacion del cilindro por su
cuadrado circunscrito: validar con un modelo mas fino que el del juego
daria por buenos huecos por los que el jugador real no cabe.
"""

import json
import os
import sys
from collections import deque

# --- Constantes tomadas del motor ---------------------------------------
# Actor: halfExtents por defecto {0.5, 0.5, 0.5} -> el jugador es una caja de
# 1x1 en el plano XZ (ver Entities/Actor.h y Entity::CollidesWithAny).
PLAYER_HALF = 0.5

# Perimetro de la arena: muros en +-16, interior jugable +-15 (ver el
# comentario de Application::DrawGroundGrid y los muros de assets/data/*.json).
ARENA_LIMIT = 16.5

# Paso de la rejilla. Tiene que ser MENOR que el grosor minimo de un
# obstaculo YA INFLADO, para que el flood-fill no pueda "atravesar" una
# pared saltandosela entre dos muestras. Cota inferior: el editor no deja
# crear un obstaculo de menos de 0.2, y la inflacion suma 1.0 -> 1.2 (el
# mas fino que hay hoy en los datos mide 0.6 -> 1.6).
STEP = 0.25


def vec(node, key, default=(0.0, 0.0, 0.0)):
    """position/size/spawn -> (x, y, z), con los mismos defaults que
    LevelLoader::ParseVector3Field."""
    n = node.get(key)
    if not isinstance(n, dict):
        return default
    return (float(n.get("x", 0.0)), float(n.get("y", 0.0)), float(n.get("z", 0.0)))


def build_blockers(level):
    """Obstaculos como AABB en XZ ya INFLADOS con el medio-tamano del jugador
    (suma de Minkowski): asi el jugador se reduce a un punto y basta con
    preguntar si el punto cae dentro de alguna caja.

    La Y se ignora a proposito: todos los obstaculos del proyecto tienen
    size.y = 3 centrado en y = 0, y la caja del jugador ([-0.5, 0.5]) siempre
    los solapa en ese eje. Reintroducirla no cambiaria ni un resultado y
    ocultaria que el problema es bidimensional.
    """
    boxes = []
    for o in level.get("obstacles", []):
        px, _, pz = vec(o, "position")
        if o.get("type") == "cylinder":
            # El motor NO usa el circulo: colisiona contra el cuadrado
            # circunscrito (ver Cylinder.h). Se replica tal cual.
            r = float(o.get("radius", 0.5))
            hx = hz = r
        else:
            sx, _, sz = vec(o, "size", (1.0, 1.0, 1.0))
            hx, hz = sx * 0.5, sz * 0.5
        boxes.append((px, pz, hx + PLAYER_HALF, hz + PLAYER_HALF))
    return boxes


def blocked(x, z, boxes):
    """AABBIntersects del motor es INCLUSIVO (tocarse cuenta como chocar),
    de ahi los <= en vez de <."""
    for cx, cz, hx, hz in boxes:
        if abs(x - cx) <= hx and abs(z - cz) <= hz:
            return True
    return False


def to_cell(x, z):
    return (int(round(x / STEP)), int(round(z / STEP)))


def flood(level, boxes):
    """BFS de 4 vecinos desde el spawn del jugador sobre la rejilla de
    celdas libres.

    4 vecinos, no 8: el motor resuelve X y Z por separado (Entity::TryMove),
    asi que una diagonal solo es transitable si al menos uno de sus dos
    caminos en L lo es -- y ese camino en L ya lo cubre la conectividad
    ortogonal. Con 8 vecinos el validador se inventaria pasos en diagonal
    entre dos esquinas que el jugador no puede cruzar.
    """
    sx, _, sz = vec(level["player"], "spawn")
    start = to_cell(sx, sz)

    limit = int(ARENA_LIMIT / STEP)
    visited = set()

    if blocked(start[0] * STEP, start[1] * STEP, boxes):
        return visited, False  # el spawn en si esta dentro de un muro

    queue = deque([start])
    visited.add(start)
    while queue:
        cx, cz = queue.popleft()
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, nz = cx + dx, cz + dz
            if (nx, nz) in visited or abs(nx) > limit or abs(nz) > limit:
                continue
            if blocked(nx * STEP, nz * STEP, boxes):
                continue
            visited.add((nx, nz))
            queue.append((nx, nz))
    return visited, True


def reachable(point, visited, boxes):
    """Un objetivo cuenta como alcanzable si alguna celda de su entorno
    inmediato (+-1 celda) esta en la region visitada: el jugador lo recoge
    por solapamiento de cajas, no pisando su centro exacto."""
    x, z = point
    cx, cz = to_cell(x, z)
    for dx in (-1, 0, 1):
        for dz in (-1, 0, 1):
            if (cx + dx, cz + dz) in visited:
                return True
    return False


def objectives(level):
    """Todo lo que el jugador TIENE que poder tocar, con su etiqueta."""
    out = []
    for i, g in enumerate(level.get("gears", [])):
        out.append((f"gears[{i}]", vec(g, "position")))
    for i, h in enumerate(level.get("healthKits", [])):
        out.append((f"healthKits[{i}]", vec(h, "position")))
    for i, p in enumerate(level.get("powerUps", [])):
        out.append((f"powerUps[{i}]", vec(p, "position")))
    if isinstance(level.get("door"), dict):
        out.append(("door", vec(level["door"], "position")))
    return out


def embedded(level, boxes):
    """Entidades clavadas DENTRO de un obstaculo. El flood-fill por si solo
    no las caza: una baldosa o un engranaje dentro de un muro sigue
    'alcanzable' si el muro toca zona libre, pero es contenido perdido.
    Se comprueba contra la caja SIN inflar (el objeto no es el jugador).
    """
    raw = []
    for o in level.get("obstacles", []):
        px, _, pz = vec(o, "position")
        if o.get("type") == "cylinder":
            r = float(o.get("radius", 0.5))
            raw.append((px, pz, r, r))
        else:
            sx, _, sz = vec(o, "size", (1.0, 1.0, 1.0))
            raw.append((px, pz, sx * 0.5, sz * 0.5))

    hits = []
    listas = ["gears", "healthKits", "powerUps", "barrels", "electricTiles", "hazards", "spawners"]
    for key in listas:
        for i, e in enumerate(level.get(key, [])):
            x, _, z = vec(e, "position")
            for cx, cz, hx, hz in raw:
                if abs(x - cx) < hx and abs(z - cz) < hz:
                    hits.append(f"{key}[{i}] en ({x}, {z}) dentro de un obstaculo")
                    break
    for i, e in enumerate(level.get("enemies", [])):
        x, _, z = vec(e, "spawn")
        for cx, cz, hx, hz in raw:
            if abs(x - cx) < hx and abs(z - cz) < hz:
                hits.append(f"enemies[{i}] en ({x}, {z}) dentro de un obstaculo")
                break
    return hits


def validate(path):
    with open(path, encoding="utf-8") as f:
        level = json.load(f)

    name = os.path.basename(path)
    if "player" not in level:
        return [f"{name}: sin 'player', el motor no puede construir la partida"]

    boxes = build_blockers(level)
    visited, spawn_ok = flood(level, boxes)

    errors = []
    if not spawn_ok:
        errors.append(f"{name}: el spawn del jugador {vec(level['player'], 'spawn')} esta dentro de un obstaculo")
        return errors

    for label, (x, _, z) in objectives(level):
        if not reachable((x, z), visited, boxes):
            errors.append(f"{name}: {label} en ({x}, {z}) es INALCANZABLE desde el spawn")

    errors += [f"{name}: {h}" for h in embedded(level, boxes)]

    if not errors:
        area = len(visited) * STEP * STEP
        print(f"OK  {name:16} {len(objectives(level)):2} objetivos alcanzables, "
              f"{area:6.1f} u2 de suelo conectado ({len(visited)} celdas)")
    return errors


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    data = os.path.join(here, "..", "assets", "data")

    if len(sys.argv) > 1:
        targets = [os.path.join(data, a if a.endswith(".json") else a + ".json") for a in sys.argv[1:]]
    else:
        targets = sorted(
            os.path.join(data, f) for f in os.listdir(data)
            if f.endswith(".json") and f != "enemy_variants.json"
        )

    errors = []
    for path in targets:
        errors += validate(path)

    print()
    if errors:
        for e in errors:
            print("FALLO " + e)
        print(f"\n{len(errors)} FALLOS")
        return 1
    print("TODO OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
