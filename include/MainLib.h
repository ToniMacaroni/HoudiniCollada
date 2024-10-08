//
// Created by Julian on 9/25/2024.
//

#ifndef OPENDRIVEPARSER_MAINLIB_H
#define OPENDRIVEPARSER_MAINLIB_H

#include <SOP/SOP_Node.h>

namespace HDK_ColladaExporter {
class GeoContainer
{
public:
    std::string m_name;
  std::vector<GA_Offset> m_primOffsets;
  std::vector<GA_Offset> m_vertOffsets;
};

class SOP_ColladaExporter : public SOP_Node
{
public:
	     SOP_ColladaExporter(OP_Network *net, const char *, OP_Operator *entry);
            ~SOP_ColladaExporter() override;

    static OP_Node	*myConstructor(OP_Network  *net, const char *name,
				       OP_Operator *entry);
    static PRM_Template	 g_myTemplateList[];


protected:
    OP_ERROR	cookMySop(OP_Context &context) override;
};
}

#endif //OPENDRIVEPARSER_MAINLIB_H
