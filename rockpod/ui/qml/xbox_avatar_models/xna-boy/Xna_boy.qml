import QtQuick
import QtQuick3D

Node {
    id: node
    property color skinTint: "white"
    property color hairTint: "white"
    property color topTint: "white"
    property color bottomTint: "white"
    property color shoesTint: "white"

    // Resources
    Texture {
        id: textures_Body__ColorMap_png_texture
        objectName: "textures/Body__ColorMap.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/Body__ColorMap.png"
    }
    Texture {
        id: textures_boy_mouth_png_texture
        objectName: "textures/boy-mouth.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/boy-mouth.png"
    }
    Texture {
        id: textures_0010_1_ColorMap_1_png_texture
        objectName: "textures/0010-1_ColorMap_1.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/0010-1_ColorMap_1.png"
    }
    Texture {
        id: textures_boy_eyebrow_png_texture
        objectName: "textures/boy-eyebrow.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/boy-eyebrow.png"
    }
    Texture {
        id: textures_Hair__ColorMap_png_texture
        objectName: "textures/Hair__ColorMap.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/Hair__ColorMap.png"
    }
    Texture {
        id: textures_boy_right_eye_png_texture
        objectName: "textures/boy-right-eye.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/boy-right-eye.png"
    }
    Texture {
        id: textures_0468_0_ColorMap_1_png_texture
        objectName: "textures/0468-0_ColorMap_1.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/0468-0_ColorMap_1.png"
    }
    Texture {
        id: textures_boy_left_eye_png_texture
        objectName: "textures/boy-left-eye.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/boy-left-eye.png"
    }
    Texture {
        id: textures_0599_0_ColorMap_1_png_texture
        objectName: "textures/0599-0_ColorMap_1.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/0599-0_ColorMap_1.png"
    }
    PrincipledMaterial {
        id: face_mouth_material
        objectName: "face_mouth"
        baseColorMap: textures_boy_mouth_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: face_brow_material
        objectName: "face_brow"
        baseColorMap: textures_boy_eyebrow_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: face_right_eye_material
        objectName: "face_right_eye"
        baseColorMap: textures_boy_right_eye_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: face_left_eye_material
        objectName: "face_left_eye"
        baseColorMap: textures_boy_left_eye_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: blendShapeEarDefault_Natal_material
        objectName: "BlendShapeEarDefault_Natal"
        baseColorMap: textures_Body__ColorMap_png_texture
        baseColor: node.skinTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: boy_top_Natal_material
        objectName: "boy_top_Natal"
        baseColorMap: textures_0599_0_ColorMap_1_png_texture
        baseColor: node.topTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: boy_shoes_Natal_material
        objectName: "boy_shoes_Natal"
        baseColorMap: textures_0468_0_ColorMap_1_png_texture
        baseColor: node.shoesTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: boy_hair_Natal_material
        objectName: "boy_hair_Natal"
        baseColorMap: textures_Hair__ColorMap_png_texture
        baseColor: node.hairTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: boy_bottoms_Natal_material
        objectName: "boy_bottoms_Natal"
        baseColorMap: textures_0010_1_ColorMap_1_png_texture
        baseColor: node.bottomTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: boy_body_mesh_Natal_material
        objectName: "boy_body_mesh_Natal"
        baseColorMap: textures_Body__ColorMap_png_texture
        baseColor: node.skinTint
        indexOfRefraction: 1
    }

    // Nodes:
    Node {
        id: xna_boy_obj
        objectName: "xna-boy.obj"
        Model {
            id: boy_body_mesh_Natal
            objectName: "boy_body_mesh_Natal"
            source: "meshes/boy_body_mesh_Natal_mesh.mesh"
            materials: [
                boy_body_mesh_Natal_material
            ]
        }
        Model {
            id: boy_bottoms_Natal
            objectName: "boy_bottoms_Natal"
            source: "meshes/boy_bottoms_Natal_mesh.mesh"
            materials: [
                boy_bottoms_Natal_material
            ]
        }
        Model {
            id: boy_hair_Natal
            objectName: "boy_hair_Natal"
            source: "meshes/boy_hair_Natal_mesh.mesh"
            materials: [
                boy_hair_Natal_material
            ]
        }
        Model {
            id: boy_shoes_Natal
            objectName: "boy_shoes_Natal"
            source: "meshes/boy_shoes_Natal_mesh.mesh"
            materials: [
                boy_shoes_Natal_material
            ]
        }
        Model {
            id: boy_top_Natal
            objectName: "boy_top_Natal"
            source: "meshes/boy_top_Natal_mesh.mesh"
            materials: [
                boy_top_Natal_material
            ]
        }
        Model {
            id: blendShapeEarDefault_Natal
            objectName: "BlendShapeEarDefault_Natal"
            source: "meshes/blendShapeEarDefault_Natal_mesh.mesh"
            materials: [
                blendShapeEarDefault_Natal_material,
                face_left_eye_material,
                face_right_eye_material,
                face_brow_material,
                face_mouth_material
            ]
        }
    }

    // Animations:
}
