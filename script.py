def clean_obj(input_file, output_file, remove_keyword):
    """
    Remove objetos de um arquivo .obj cujo nome contenha determinada palavra.
    Reindexa os vértices e mantém consistência.

    input_file: arquivo de entrada (.obj)
    output_file: arquivo de saída (.obj limpo)
    remove_keyword: string que, se aparecer no nome do objeto/grupo, faz ele ser removido
                    (exemplo: "Window" remove "Window01", "BigWindow", etc.)
    """

    vertices, texcoords, normals = [], [], []
    faces = []
    mtllibs = []
    current_obj = None
    current_mtl = None

    with open(input_file, 'r', encoding="utf-8") as f:
        lines = f.readlines()

    for line in lines:
        if line.startswith("mtllib "):
            mtllibs.append(line.strip())

        elif line.startswith("usemtl "):
            current_mtl = line.strip()

        elif line.startswith(("o ", "g ")):
            current_obj = line.strip().split(" ", 1)[1]

        elif line.startswith("v "):
            vertices.append(line.strip())

        elif line.startswith("vt "):
            texcoords.append(line.strip())

        elif line.startswith("vn "):
            normals.append(line.strip())

        elif line.startswith("f "):
            # só mantém se o nome do objeto NÃO contém a palavra-chave
            if not (current_obj and remove_keyword.lower() in current_obj.lower()):
                faces.append((current_obj, current_mtl, line.strip()))

    # Descobrir índices usados
    used_vertices, used_tex, used_normals = set(), set(), set()
    for _, _, face in faces:
        parts = face.split()[1:]
        for p in parts:
            vals = p.split("/")
            if vals[0]:
                used_vertices.add(int(vals[0]))
            if len(vals) > 1 and vals[1]:
                used_tex.add(int(vals[1]))
            if len(vals) > 2 and vals[2]:
                used_normals.add(int(vals[2]))

    # Reindexação
    vmap = {old: new for new, old in enumerate(sorted(used_vertices), start=1)}
    vtmap = {old: new for new, old in enumerate(sorted(used_tex), start=1)}
    vnmap = {old: new for new, old in enumerate(sorted(used_normals), start=1)}

    # Escrevendo novo arquivo
    with open(output_file, "w", encoding="utf-8") as f:
        # mtllibs
        for m in mtllibs:
            f.write(m + "\n")

        # vértices
        for old_idx in sorted(used_vertices):
            f.write(vertices[old_idx - 1] + "\n")
        for old_idx in sorted(used_tex):
            f.write(texcoords[old_idx - 1] + "\n")
        for old_idx in sorted(used_normals):
            f.write(normals[old_idx - 1] + "\n")

        # faces com material
        last_mtl = None
        for obj_name, mtl, face in faces:
            if mtl != last_mtl and mtl is not None:
                f.write(mtl + "\n")
                last_mtl = mtl

            parts = face.split()[1:]
            new_face = []
            for p in parts:
                vals = p.split("/")
                v = vmap.get(int(vals[0]), None) if vals[0] else ""
                vt = vtmap.get(int(vals[1]), None) if len(vals) > 1 and vals[1] else ""
                vn = vnmap.get(int(vals[2]), None) if len(vals) > 2 and vals[2] else ""
                new_str = str(v) if v else ""
                if vt or vn:
                    new_str += "/" + (str(vt) if vt else "")
                if vn:
                    new_str += "/" + str(vn)
                new_face.append(new_str)
            f.write("f " + " ".join(new_face) + "\n")

    print(f"Novo arquivo salvo em {output_file} (objetos removidos contendo: '{remove_keyword}')")


if __name__ == "__main__":
    input_obj = "CLASSROOM.obj"
    output_obj = "CLASSROOM_.obj"
    keyword = "Cube.001"

    clean_obj(input_obj, output_obj, keyword)

