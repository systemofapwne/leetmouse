#ifndef GUI_CONFIGHELPER_H
#define GUI_CONFIGHELPER_H

#include "DriverHelper.h"

namespace ConfigHelper {
    std::string ExportPlainText(Parameters params, bool save_to_file);

    std::string ExportConfig(Parameters params, bool save_to_file);

    bool ImportFile(char *lut_data, Parameters &params);

    bool ImportClipboard(char *lut_data, const char *clipboard, Parameters &params);
} // ConfigHelper

#endif //GUI_CONFIGHELPER_H
