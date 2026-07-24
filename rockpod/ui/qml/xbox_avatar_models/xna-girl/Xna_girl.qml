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
        id: textures_girl_mouth_png_texture
        objectName: "textures/girl-mouth.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/girl-mouth.png"
    }
    Texture {
        id: textures_0334_0_ColorMap_1_png_texture
        objectName: "textures/0334-0_ColorMap_1.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/0334-0_ColorMap_1.png"
    }
    Texture {
        id: textures_girl_eyebrow_png_texture
        objectName: "textures/girl-eyebrow.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/girl-eyebrow.png"
    }
    Texture {
        id: textures_0687_0_ColorMap_1_png_texture
        objectName: "textures/0687-0_ColorMap_1.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/0687-0_ColorMap_1.png"
    }
    Texture {
        id: textures_girl_right_eye_png_texture
        objectName: "textures/girl-right-eye.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/girl-right-eye.png"
    }
    Texture {
        id: textures_0320_0_ColorMap_1_png_texture
        objectName: "textures/0320-0_ColorMap_1.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/0320-0_ColorMap_1.png"
    }
    Texture {
        id: textures_girl_left_eye_png_texture
        objectName: "textures/girl-left-eye.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/girl-left-eye.png"
    }
    Texture {
        id: textures_GirlHair__ColorMap_png_texture
        objectName: "textures/GirlHair__ColorMap.png"
        generateMipmaps: true
        mipFilter: Texture.Linear
        source: "maps/GirlHair__ColorMap.png"
    }
    PrincipledMaterial {
        id: face_mouth_material
        objectName: "face_mouth"
        baseColorMap: textures_girl_mouth_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: face_brow_material
        objectName: "face_brow"
        baseColorMap: textures_girl_eyebrow_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: face_right_eye_material
        objectName: "face_right_eye"
        baseColorMap: textures_girl_right_eye_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: face_left_eye_material
        objectName: "face_left_eye"
        baseColorMap: textures_girl_left_eye_png_texture
        alphaMode: PrincipledMaterial.Blend
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: girlHead_BlendShapeEarDefault_Natal_material
        objectName: "girlHead_BlendShapeEarDefault_Natal"
        baseColorMap: textures_Body__ColorMap_png_texture
        baseColor: node.skinTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: girl_hair_Natal_material
        objectName: "girl_hair_Natal"
        baseColorMap: textures_GirlHair__ColorMap_png_texture
        baseColor: node.hairTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: girl_top_Natal_material
        objectName: "girl_top_Natal"
        baseColorMap: textures_0320_0_ColorMap_1_png_texture
        baseColor: node.topTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: girl_bottoms_Natal_material
        objectName: "girl_bottoms_Natal"
        baseColorMap: textures_0687_0_ColorMap_1_png_texture
        baseColor: node.bottomTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: girl_shoes_Natal_material
        objectName: "girl_shoes_Natal"
        baseColorMap: textures_0334_0_ColorMap_1_png_texture
        baseColor: node.shoesTint
        indexOfRefraction: 1
    }
    PrincipledMaterial {
        id: girl_body_mesh_Natal_material
        objectName: "girl_body_mesh_Natal"
        baseColorMap: textures_Body__ColorMap_png_texture
        baseColor: node.skinTint
        indexOfRefraction: 1
    }

    // Nodes:
    Node {
        id: xna_girl_obj
        objectName: "xna-girl.obj"
        Model {
            id: girl_body_mesh_Natal
            objectName: "girl_body_mesh_Natal"
            source: "meshes/girl_body_mesh_Natal_mesh.mesh"
            materials: [
                girl_body_mesh_Natal_material
            ]
        }
        Model {
            id: girl_shoes_Natal
            objectName: "girl_shoes_Natal"
            source: "meshes/girl_shoes_Natal_mesh.mesh"
            materials: [
                girl_shoes_Natal_material
            ]
        }
        Model {
            id: girl_bottoms_Natal
            objectName: "girl_bottoms_Natal"
            source: "meshes/girl_bottoms_Natal_mesh.mesh"
            materials: [
                girl_bottoms_Natal_material
            ]
        }
        Model {
            id: girl_top_Natal
            objectName: "girl_top_Natal"
            source: "meshes/girl_top_Natal_mesh.mesh"
            materials: [
                girl_top_Natal_material
            ]
        }
        Model {
            id: girl_hair_Natal
            objectName: "girl_hair_Natal"
            source: "meshes/girl_hair_Natal_mesh.mesh"
            materials: [
                girl_hair_Natal_material
            ]
        }
        Model {
            id: girlHead_BlendShapeEarDefault_Natal
            objectName: "girlHead_BlendShapeEarDefault_Natal"
            source: "meshes/girlHead_BlendShapeEarDefault_Natal_mesh.mesh"
            materials: [
                girlHead_BlendShapeEarDefault_Natal_material,
                face_left_eye_material,
                face_right_eye_material,
                face_brow_material,
                face_mouth_material
            ]
        }
    }

    // Animations:
}
