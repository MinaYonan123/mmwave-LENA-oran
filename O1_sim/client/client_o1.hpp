#include <algorithm>
#include <curl/curl.h>
#include "pugixml.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static size_t WriteCallback(void *, size_t, size_t, std::string *);

std::string FetchFilteredConfig(const std::string &, const std::string &,
                                const std::string &);

std::string FetchConfigXml(const std::string &, const std::string &);

bool stob(std::string);

void trim(std::string &);

std::string convertToCondition(const std::string &);

struct ConfigData {
  std::string filename;
  std::string condition;
};

ConfigData read_config(const std::string &  = "scratch/config");

class Entity {
private:
  std::map<std::string, bool> defined = {};

  std::string _cell_name = "";
  int _X = 0;
  int _Y = 0;
  int _azimut = 0;
  int _tilt = 0;
  double _hba = 0;
  int _sector = 0;
  int _gnodeb_id = 0;
  bool _advancedDlSuMimoEnabled = false;
  int _csiRsPeriodicity = 0;
  int _dlMaxMuMimoLayers = 0;
  int _ssbSubCarrierSpacing = 0;
  int _bandListManual = 0;
  int _srsPeriodicity = 0;
  double _pdcchLaBfGainFraction = 0;
  std::string _srsHoppingBandwidth = "";
  std::string _csiReportFormat = "";
  bool _dl256QamEnabled = false;
  std::string _csiRsActivePortConfig = "";
  int _cbrsCellBandwidthPreferred = 0;
  std::string _energySavingControl = "";

public:
  std::string set_from_xml(pugi::xml_node entry) {

    if (entry.child("cell_name")) {
      _cell_name = entry.child("cell_name").text().as_string();
      defined["cell_name"] = true;
    } else {
      std::cout << "cell_name error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("X")) {
      try {
        _X = std::stoi(entry.child("X").text().as_string());
        defined["X"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "X error in set_from_xml for cell_name = " << _cell_name
                  << std::endl;
      }
    } else {
      std::cout << "X error in set_from_xml for cell_name = " << _cell_name
                << std::endl;
    }

    if (entry.child("Y")) {
      try {
        _Y = std::stoi(entry.child("Y").text().as_string());
        defined["Y"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "Y error in set_from_xml for cell_name = " << _cell_name
                  << std::endl;
      }
    } else {
      std::cout << "Y error in set_from_xml for cell_name = " << _cell_name
                << std::endl;
    }

    if (entry.child("azimut")) {
      try {
        _azimut = std::stoi(entry.child("azimut").text().as_string());
        defined["azimut"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "azimut error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "azimut error in set_from_xml for cell_name = " << _cell_name
                << std::endl;
    }

    if (entry.child("tilt")) {
      try {
        _tilt = std::stoi(entry.child("tilt").text().as_string());
        defined["tilt"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "tilt error in set_from_xml for cell_name = " << _cell_name
                  << std::endl;
      }
    } else {
      std::cout << "tilt error in set_from_xml for cell_name = " << _cell_name
                << std::endl;
    }

    if (entry.child("sector")) {
      try {
        _sector = std::stoi(entry.child("sector").text().as_string());
        defined["sector"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "sector error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "sector error in set_from_xml for cell_name = " << _cell_name
                << std::endl;
    }

    if (entry.child("gnodeb_id")) {
      try {
        _gnodeb_id = std::stoi(entry.child("gnodeb_id").text().as_string());
        defined["gnodeb_id"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "gnodeb_id error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "gnodeb_id error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

      if (entry.child("csiRsPeriodicity")) {
          std::string value = entry.child("csiRsPeriodicity").text().as_string();
          if (value.empty()) {
              std::cout << "[DEBUG] csiRsPeriodicity node exists but is empty for cell_name = "
                        << _cell_name << std::endl;
          } else {
              try {
                  _csiRsPeriodicity = std::stoi(value);
                  defined["csiRsPeriodicity"] = true;
              } catch (const std::invalid_argument &e) {
                  std::cout << "[ERROR] Invalid argument while parsing csiRsPeriodicity: '"
                            << value << "' for cell_name = " << _cell_name
                            << " (" << e.what() << ")" << std::endl;
              } catch (const std::out_of_range &e) {
                  std::cout << "[ERROR] Out of range while parsing csiRsPeriodicity: '"
                            << value << "' for cell_name = " << _cell_name
                            << " (" << e.what() << ")" << std::endl;
              }
          }
      } else {
          std::cout << "[DEBUG] csiRsPeriodicity node missing for cell_name = "
                    << _cell_name << std::endl;
      }


    if (entry.child("hba")) {
      try {
        _hba = std::stod(entry.child("hba").text().as_string());
        defined["hba"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "hba error in set_from_xml for cell_name = " << _cell_name
                  << std::endl;
      }
    } else {
      std::cout << "hba error in set_from_xml for cell_name = " << _cell_name
                << std::endl;
    }

    if (entry.child("advancedDlSuMimoEnabled")) {
      try {
        _advancedDlSuMimoEnabled =
            stob(entry.child("advancedDlSuMimoEnabled").text().as_string());
        defined["advancedDlSuMimoEnabled"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout
            << "advancedDlSuMimoEnabled error in set_from_xml for cell_name = "
            << _cell_name << std::endl;
      }
    } else {
      std::cout
          << "advancedDlSuMimoEnabled error in set_from_xml for cell_name = "
          << _cell_name << std::endl;
    }

      if (entry.child("csiRsPeriodicity")) {
          std::string value = entry.child("csiRsPeriodicity").text().as_string();
          if (value.empty()) {
              std::cout << "[DEBUG] csiRsPeriodicity node exists but is empty for cell_name = "
                        << _cell_name << std::endl;
          } else {
              try {
                  _csiRsPeriodicity = std::stoi(value);
                  defined["csiRsPeriodicity"] = true;
              } catch (const std::invalid_argument &e) {
                  std::cout << "[ERROR] Invalid argument while parsing csiRsPeriodicity: '"
                            << value << "' for cell_name = " << _cell_name
                            << " (" << e.what() << ")" << std::endl;
              } catch (const std::out_of_range &e) {
                  std::cout << "[ERROR] Out of range while parsing csiRsPeriodicity: '"
                            << value << "' for cell_name = " << _cell_name
                            << " (" << e.what() << ")" << std::endl;
              }
          }
      } else {
          std::cout << "[DEBUG] csiRsPeriodicity node missing for cell_name = "
                    << _cell_name << std::endl;
      }


    if (entry.child("dlMaxMuMimoLayers")) {
      try {
        _dlMaxMuMimoLayers =
            std::stoi(entry.child("dlMaxMuMimoLayers").text().as_string());
        defined["dlMaxMuMimoLayers"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "dlMaxMuMimoLayers error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "dlMaxMuMimoLayers error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("ssbSubCarrierSpacing")) {
      try {
        _ssbSubCarrierSpacing =
            std::stoi(entry.child("ssbSubCarrierSpacing").text().as_string());
        defined["ssbSubCarrierSpacing"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout
            << "ssbSubCarrierSpacing error in set_from_xml for cell_name = "
            << _cell_name << std::endl;
      }
    } else {
      std::cout << "ssbSubCarrierSpacing error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("bandListManual")) {
      try {
        _bandListManual =
            std::stoi(entry.child("bandListManual").text().as_string());
        defined["bandListManual"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "bandListManual error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "bandListManual error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("srsPeriodicity")) {
      try {
        _srsPeriodicity =
            std::stoi(entry.child("srsPeriodicity").text().as_string());
        defined["srsPeriodicity"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "srsPeriodicity error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "srsPeriodicity error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("pdcchLaBfGainFraction")) {
      try {
        _pdcchLaBfGainFraction =
            std::stod(entry.child("pdcchLaBfGainFraction").text().as_string());
        defined["pdcchLaBfGainFraction"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout
            << "pdcchLaBfGainFraction error in set_from_xml for cell_name = "
            << _cell_name << std::endl;
      }
    } else {
      std::cout
          << "pdcchLaBfGainFraction error in set_from_xml for cell_name = "
          << _cell_name << std::endl;
    }

    if (entry.child("srsHoppingBandwidth")) {
      _srsHoppingBandwidth =
          entry.child("srsHoppingBandwidth").text().as_string();
      defined["srsHoppingBandwidth"] = true;
    } else {
      std::cout << "srsHoppingBandwidth error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("csiReportFormat")) {
      _csiReportFormat = entry.child("csiReportFormat").text().as_string();
      defined["csiReportFormat"] = true;
    } else {
      std::cout << "csiReportFormat error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("dl256QamEnabled")) {
      try {
        _dl256QamEnabled =
            stob(entry.child("dl256QamEnabled").text().as_string());
        defined["dl256QamEnabled"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "dl256QamEnabled error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "dl256QamEnabled error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    if (entry.child("csiRsActivePortConfig")) {
      _csiRsActivePortConfig =
          entry.child("csiRsActivePortConfig").text().as_string();
      defined["csiRsActivePortConfig"] = true;
    } else {
      std::cout
          << "csiRsActivePortConfig error in set_from_xml for cell_name = "
          << _cell_name << std::endl;
    }

    if (entry.child("cbrsCellBandwidthPreferred")) {
      try {
        _cbrsCellBandwidthPreferred = std::stoi(
            entry.child("cbrsCellBandwidthPreferred").text().as_string());
        defined["cbrsCellBandwidthPreferred"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "cbrsCellBandwidthPreferred error in set_from_xml for "
                     "cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout
          << "cbrsCellBandwidthPreferred error in set_from_xml for cell_name = "
          << _cell_name << std::endl;
    }

    if (entry.child("energySavingControl")) {
      try {
        _energySavingControl =
            entry.child("energySavingControl").text().as_string();
        defined["energySavingControl"] = true;
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid argument: " << e.what() << std::endl;
        std::cout << "energySavingControl error in set_from_xml for cell_name = "
                  << _cell_name << std::endl;
      }
    } else {
      std::cout << "on error in set_from_xml for cell_name = "
                << _cell_name << std::endl;
    }

    return _cell_name;
  }

  std::string cell_name() {
    if (!defined["cell_name"])
      std::cout << " cell_name error " << std::endl;
    return _cell_name;
  }
  int X() {
    if (!defined["X"])
      std::cout << " X error " << std::endl;
    return _X;
  }
  int Y() {
    if (!defined["Y"])
      std::cout << " Y error " << std::endl;
    return _Y;
  }
  int azimut() {
    if (!defined["azimut"])
      std::cout << " azimut error " << std::endl;
    return _azimut;
  }
  int tilt() {
    if (!defined["tilt"])
      std::cout << " tilt error " << std::endl;
    return _tilt;
  }
  int sector() {
    if (!defined["sector"])
      std::cout << " sector error " << std::endl;
    return _sector;
  }
  int gnodeb_id() {
    if (!defined["gnodeb_id"])
      std::cout << " gnodeb_id error " << std::endl;
    return _gnodeb_id;
  }
  double hba() {
    if (!defined["hba"])
      std::cout << " hba error " << std::endl;
    return _hba;
  }
  bool advancedDlSuMimoEnabled() {
    if (!defined["advancedDlSuMimoEnabled"])
      std::cout << " advancedDlSuMimoEnabled error " << std::endl;
    return _advancedDlSuMimoEnabled;
  }
  int csiRsPeriodicity() {
    if (!defined["csiRsPeriodicity"])
      std::cout << " csiRsPeriodicity error " << std::endl;
    return _csiRsPeriodicity;
  }
  int dlMaxMuMimoLayers() {
    if (!defined["dlMaxMuMimoLayers"])
      std::cout << " dlMaxMuMimoLayers error " << std::endl;
    return _dlMaxMuMimoLayers;
  }
  int ssbSubCarrierSpacing() {
    if (!defined["ssbSubCarrierSpacing"])
      std::cout << " ssbSubCarrierSpacing error " << std::endl;
    return _ssbSubCarrierSpacing;
  }
  int bandListManual() {
    if (!defined["bandListManual"])
      std::cout << " bandListManual error " << std::endl;
    return _bandListManual;
  }
  int srsPeriodicity() {
    if (!defined["srsPeriodicity"])
      std::cout << " srsPeriodicity error " << std::endl;
    return _srsPeriodicity;
  }
  double pdcchLaBfGainFraction() {
    if (!defined["pdcchLaBfGainFraction"])
      std::cout << " pdcchLaBfGainFraction error " << std::endl;
    return _pdcchLaBfGainFraction;
  }
  std::string srsHoppingBandwidth() {
    if (!defined["srsHoppingBandwidth"])
      std::cout << " srsHoppingBandwidth error " << std::endl;
    return _srsHoppingBandwidth;
  }
  std::string csiReportFormat() {
    if (!defined["csiReportFormat"])
      std::cout << " csiReportFormat error " << std::endl;
    return _csiReportFormat;
  }
  bool dl256QamEnabled() {
    if (!defined["dl256QamEnabled"])
      std::cout << " dl256QamEnabled error " << std::endl;
    return _dl256QamEnabled;
  }
  std::string csiRsActivePortConfig() {
    if (!defined["csiRsActivePortConfig"])
      std::cout << " csiRsActivePortConfig error " << std::endl;
    return _csiRsActivePortConfig;
  }
  int cbrsCellBandwidthPreferred() {
    if (!defined["cbrsCellBandwidthPreferred"])
      std::cout << " cbrsCellBandwidthPreferred error " << std::endl;
    return _cbrsCellBandwidthPreferred;
  }
  std::string energySavingControl() {
    if (!defined["energySavingControl"])
      std::cout << " energySavingControl  error " << std::endl;
    return _energySavingControl;
  }

};

