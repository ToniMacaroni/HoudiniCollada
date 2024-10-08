#include "MainLib.h"

#include <GU/GU_Detail.h>
#include <GA/GA_Handle.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <UT/UT_DSOVersion.h>

#include <GEO/GEO_IOTranslator.h>
#include <UT/UT_Vector3.h>

#include <GEO/GEO_Detail.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_PrimVolume.h>
#include <CH/CH_Manager.h>

#include "pugixml/pugixml.hpp"

using namespace HDK_ColladaExporter;
using namespace pugi;

void
newSopOperator(OP_OperatorTable *table)
{
    table->addOperator(new OP_Operator(
        "collada_exporter",
        "Collada Exporter",
        SOP_ColladaExporter::myConstructor,
        SOP_ColladaExporter::g_myTemplateList,
        1,
        1));
}

static std::string
convertToString(const std::vector<UT_Vector3>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        oss << data[i].x() << " " << data[i].y() << " " << data[i].z();
        if (i != data.size() - 1) {
            oss << " ";
        }
    }
    return oss.str();
}

static std::string
convertToString(const std::vector<UT_Vector2>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        oss << data[i].x() << " " << data[i].y();
        if (i != data.size() - 1) {
            oss << " ";
        }
    }
    return oss.str();
}

static std::string
convertToString(const std::vector<int>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        oss << data[i];
        if (i != data.size() - 1) {
            oss << " ";
        }
    }
    return oss.str();
}

static xml_node
createSceneNode(xml_node& parent, const std::string& id)
{
    xml_node node = parent.append_child("node");
    node.append_attribute("id") = id.c_str();
    node.append_attribute("name") = id.c_str();
    node.append_attribute("type") = "NODE";

    xml_node matrix = node.append_child("matrix");
    matrix.append_attribute("sid") = "transform";
    matrix.text().set("1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1");
    return node;
}

static void
bindMaterial(xml_node& node, const std::string& material)
{
    xml_node bind_material = node.append_child("bind_material");
    xml_node technique_common = bind_material.append_child("technique_common");
    xml_node instance_material = technique_common.append_child("instance_material");
    instance_material.append_attribute("symbol") = material.c_str();
    //instance_material.append_attribute("target") = material.c_str();
}

static void
addTechnique(xml_node & source_node, int count, const char* sourceName, int stride, const std::string& fieldNames){
    xml_node technique_uv = source_node.append_child("technique_common");
    xml_node accessor_uv = technique_uv.append_child("accessor");
    accessor_uv.append_attribute("source") = sourceName;
    accessor_uv.append_attribute("count") = count;
    accessor_uv.append_attribute("stride") = stride;

    for (int i = 0; i < fieldNames.size(); ++i) {
        xml_node param_uv = accessor_uv.append_child("param");
        param_uv.append_attribute("name") = fieldNames.substr(i, 1).c_str();
        param_uv.append_attribute("type") = "float";
    }
}

static void
addSource(xml_node& mesh, const std::string& id, const std::string& name, const std::string& data, int count, int stride, const std::string& fieldNames)
{
    xml_node source = mesh.append_child("source");
    source.append_attribute("id") = id.c_str();
    source.append_attribute("name") = name.c_str();
    xml_node float_array = source.append_child("float_array");
    float_array.append_attribute("id") = (id + "-array").c_str();
    float_array.append_attribute("count") = count * stride;
    float_array.text().set(data.c_str());
    addTechnique(source, count, ("#" + id + "-array").c_str(), stride, fieldNames);
}

static void
addTriangleInput(xml_node& triangles_node, const std::string& semantic, const std::string& source, int offset)
{
    xml_node input = triangles_node.append_child("input");
    input.append_attribute("semantic") = semantic.c_str();
    input.append_attribute("source") = source.c_str();
    input.append_attribute("offset") = offset;
}

static int
exportGeoData(void *data, int index, fpreal t, const PRM_Template *tplate)
{
    auto *sop = static_cast<SOP_ColladaExporter *>(data);

    UT_String filePath;
    sop->evalString(filePath, "file", 0, t);

    if(filePath == "")
        return 0;

    if(!filePath.endsWith(".dae"))
    {
        filePath += ".dae";
    }

    OP_Context  context(0);
    GU_DetailHandle gdHandle = sop->getCookedGeoHandle(context);

    if(!gdHandle.isValid())
        return 0;

    const GU_Detail* gdp = gdHandle.gdp();

    std::unordered_map<UT_StringHolder, GeoContainer> geos;

    const GA_Attribute *nameAttr = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "name", 1);
    const GA_Attribute *materialAttr = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "shop_materialpath", 1);
    const GA_Attribute *uvAttrib = gdp->findFloatTuple(GA_ATTRIB_POINT, "uv", 3);
    const GA_Attribute *normalAttrib = gdp->findFloatTuple(GA_ATTRIB_POINT, "N", 3);

    uvAttrib = nullptr;
    normalAttrib = nullptr;

    GA_ROHandleS *p_nameHandle = nullptr;
    if (nameAttr)
    {
        GA_ROHandleS nameHandle(nameAttr);
        p_nameHandle = &nameHandle;
    }

    auto getName = [&](GA_Offset offset) -> UT_StringHolder {
        if (p_nameHandle)
            return p_nameHandle->get(offset);
        return "default";
    };

    UT_Debug dbg("HoudiniCollada");

    std::string lastGeoName = "";

    for(GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
        const GA_Primitive *prim = gdp->getPrimitive(*it);
        if (prim->getTypeId() == GA_PRIMPOLY)
        {
            UT_StringHolder geoName = getName(*it);
            if (geoName != lastGeoName)
            {
                lastGeoName = geoName;
                geos[geoName].m_name = geoName.toStdString();
            }

            GeoContainer& container = geos[geoName];
            container.m_primOffsets.push_back(*it);

            for (GA_Size i = 0; i < prim->getVertexCount(); ++i)
            {
                GA_Offset vtx = prim->getVertexOffset(i);
                if (std::find(container.m_vertOffsets.begin(), container.m_vertOffsets.end(), vtx) == container.m_vertOffsets.end())
                {
                    container.m_vertOffsets.push_back(vtx);
                }
            }
        }
    }

    GA_ROHandleS* p_materialHandle = nullptr;
    GA_ROHandleV3* p_uvHandle = nullptr;
    GA_ROHandleV3* p_normalHandle = nullptr;

    if(materialAttr)
    {
        GA_ROHandleS materialHandle(materialAttr);
        p_materialHandle = &materialHandle;
    }

    bool isPointUV = true;
    bool isPointNormal = true;
    if (!uvAttrib)
    {
        uvAttrib = gdp->findFloatTuple(GA_ATTRIB_VERTEX, "uv", 3);
        isPointUV = false;
    }
    if (!normalAttrib)
    {
        normalAttrib = gdp->findFloatTuple(GA_ATTRIB_VERTEX, "N", 3);
        isPointNormal = false;
    }

    if(uvAttrib)
    {
        GA_ROHandleV3 uvHandle(uvAttrib);
        p_uvHandle = &uvHandle;
    }
    if(normalAttrib)
    {
        GA_ROHandleV3 normalHandle(normalAttrib);
        p_normalHandle = &normalHandle;
    }

    xml_document doc;
    xml_node collada = doc.append_child("COLLADA");
    xml_node geoms = collada.append_child("library_geometries");

    for(auto& geo : geos)
    {
        auto& container = geo.second;
        auto& geoName = container.m_name;

        xml_node geom = geoms.append_child("geometry");
        geom.append_attribute("id") = geoName.c_str();
        geom.append_attribute("name") = geoName.c_str();
        xml_node mesh = geom.append_child("mesh");

        xml_node sourcePos = mesh.append_child("source");
        sourcePos.append_attribute("id") = (geoName + "-mesh-positions").c_str();
        xml_node floatArrayPos = sourcePos.append_child("float_array");
        floatArrayPos.append_attribute("id") = (geoName + "-mesh-positions-array").c_str();

        std::vector<UT_Vector3> positions(container.m_vertOffsets.size());
        std::vector<UT_Vector2> uvs;
        std::vector<UT_Vector3> normals;
        std::vector<int> polyIndices;

        for (uint i = 0; i < container.m_vertOffsets.size(); ++i)
        {
            positions[i] = gdp->getPos3(gdp->vertexPoint(container.m_vertOffsets[i]));
        }

        // for (GA_Offset primOffset : container.m_primOffsets) {
        //     const GA_Primitive* prim = gdp->getPrimitive(primOffset);
        //     for (GA_Size i = prim->getVertexCount() - 1; i >= 0; --i) {
        //         GA_Offset vtx = prim->getVertexOffset(i);
        //         //indices.push_back(gdp->pointIndex(gdp->vertexPoint(*std::find(container.m_vertOffsets.begin(), container.m_vertOffsets.end(), vtx))));
        //         indices.push_back(std::distance(container.m_vertOffsets.begin(), std::find(container.m_vertOffsets.begin(), container.m_vertOffsets.end(), vtx)));
        //     }
        // }

        for (int i = 0; i < container.m_primOffsets.size(); ++i)
        {
            int vertCount = gdp->getPrimitive(container.m_primOffsets[i])->getVertexCount();
            for (int y = vertCount - 1; y >= 0; --y)
            {
                int idx = i * 3 + y;
                polyIndices.push_back(idx);
                if (uvAttrib)
                    polyIndices.push_back(idx);
                if (normalAttrib)
                    polyIndices.push_back(idx);
            }

            for (int y = 0; y < vertCount; ++y)
            {
                int idx = i * 3 + y;
                if (uvAttrib)
                {
                    const UT_Vector3 uv = p_uvHandle->get(isPointUV ? gdp->vertexPoint(container.m_vertOffsets[idx]) : container.m_vertOffsets[idx]);
                    uvs.emplace_back(uv.x(), uv.y());
                }
                if (normalAttrib)
                {
                    const UT_Vector3 normal = p_normalHandle->get(isPointNormal ? gdp->vertexPoint(container.m_vertOffsets[idx]) : container.m_vertOffsets[idx]);
                    normals.push_back(normal);
                }
            }
        }

        floatArrayPos.append_attribute("count") = positions.size() * 3;
        floatArrayPos.text().set(convertToString(positions).c_str());

        addTechnique(sourcePos, positions.size(), ("#" + geoName + "-mesh-positions-array").c_str(), 3, "XYZ");
        if(normalAttrib)
            addSource(mesh, geoName + "-mesh-normals", geoName + "-mesh-normals", convertToString(normals), normals.size(), 3, "XYZ");

        if(uvAttrib)
            addSource(mesh, geoName + "-mesh-map-0", geoName + "-mesh-map-0", convertToString(uvs), uvs.size(), 2, "ST");

        xml_node vertices = mesh.append_child("vertices");
        vertices.append_attribute("id") = (geoName + "-mesh-vertices").c_str();
        xml_node input = vertices.append_child("input");
        input.append_attribute("semantic") = "POSITION";
        input.append_attribute("source") = ("#" + geoName + "-mesh-positions").c_str();

        xml_node triangleNode = mesh.append_child("triangles");
        triangleNode.append_attribute("count") = gdp->getNumPrimitives();
        triangleNode.append_attribute("material") = p_materialHandle ? p_materialHandle->get(container.m_primOffsets[0]).c_str() : "default";

        addTriangleInput(triangleNode, "VERTEX", "#" + geoName + "-mesh-vertices", 0);
        if(normalAttrib)
            addTriangleInput(triangleNode, "NORMAL", "#" + geoName + "-mesh-normals", 1);
        if(uvAttrib)
            addTriangleInput(triangleNode, "TEXCOORD", "#" + geoName + "-mesh-map-0", 2);

        xml_node p = triangleNode.append_child("p");

        p.text().set(convertToString(polyIndices).c_str());
    }

    xml_node libraryVisualScenes = collada.append_child("library_visual_scenes");
    xml_node visualScenes = libraryVisualScenes.append_child("visual_scene");
    visualScenes.append_attribute("id") = "visual_scene0";
    visualScenes.append_attribute("name") = "visual_scene0";
    xml_node baseNode = createSceneNode(visualScenes, "base00");
    xml_node startNode = createSceneNode(baseNode, "start01");

    for(auto& geo : geos)
    {
        xml_node geoName = createSceneNode(startNode, "geonode");
        xml_node instanceGeometry = geoName.append_child("instance_geometry");
        instanceGeometry.append_attribute("url") = ("#" + geo.second.m_name).c_str();
        instanceGeometry.append_attribute("name") = geo.second.m_name.c_str();
        bindMaterial(instanceGeometry, p_materialHandle ? p_materialHandle->get(geo.second.m_primOffsets[0]).toStdString() : "default");
    }

    xml_node scene = collada.append_child("scene");
    scene.append_child("instance_visual_scene").append_attribute("url") = "#visual_scene0";

    doc.save_file(filePath.c_str());

    return 0;
}

static PRM_Name
sop_names[] = {
    PRM_Name("file",      	"File"),
    PRM_Name("export",      	"Export"),
};

PRM_Template
SOP_ColladaExporter::g_myTemplateList[]=
{
    PRM_Template(PRM_FILE, 1, &sop_names[0], 0),
    PRM_Template(PRM_CALLBACK, 1, &sop_names[1], 0, 0, 0, exportGeoData),
    PRM_Template()
};

OP_Node *
SOP_ColladaExporter::myConstructor(OP_Network *net,const char *name,OP_Operator *entry)
{
    return new SOP_ColladaExporter(net, name, entry);
}

SOP_ColladaExporter::SOP_ColladaExporter(OP_Network *net, const char *name, OP_Operator *entry)
    : SOP_Node(net, name, entry)
{
    mySopFlags.setManagesDataIDs(true);
}

SOP_ColladaExporter::~SOP_ColladaExporter()
= default;


OP_ERROR
SOP_ColladaExporter::cookMySop(OP_Context &context)
{
    fpreal t = context.getTime();

    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();

    duplicateSource(0, context);

    return error();
}

