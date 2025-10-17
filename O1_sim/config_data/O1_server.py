import http.server
import socketserver
import xml.etree.ElementTree as ET
import os
import csv
import time
import re
import json
from dicttoxml import dicttoxml
from threading import Thread
from boolean import BooleanAlgebra
from xml.dom.minidom import parseString
import xml.dom.minidom

PORT = 8831
CSV_DIR = "csv"
XML_DIR = "xml"

def quote(name):
    return "\""+name.strip()+"\""

def entity_simple(fout, klucz, wartosc, last):
    wartosc = wartosc.replace("\"","")
    if last:
        fout[0] += klucz+": "+quote(wartosc)+"\n"
    else:
        fout[0] += klucz+": "+quote(wartosc)+",\n"

def entity_simple_in(el):
    el_collected = el[:]
    if ":" in el_collected:
        klucz = el_collected[0:el_collected.index(":")]
        wartosc = el_collected[el_collected.index(":")+1:]
    wartosc = wartosc.replace("\"\"","\"")
    return "\t"+quote(klucz)+": "+quote(wartosc.strip())

def entity_list_in(el):
    result = "\t{"
    result_list=[]
    klucz=""
    wartosc=""
    el_collected = el[:]
    if "," in el_collected:
        el_collected_list = el_collected.split(",")
        for el in el_collected_list:
            klucz = el[0:el.index(":")]
            wartosc = el[el.index(":")+1:]
            result_list += [quote(klucz)+": "+quote(wartosc.strip())]
        result += ", ".join(result_list)
    else:
        el = el[:]
        if el.find(":")>=0:
            klucz = el[0:el.index(":")]
            wartosc = el[el.index(":")+1:]

        result += quote(klucz)+": "+quote(wartosc.strip())

    return result+"}"

def entity_with_sub(fout, klucz, wartosc, last):

    wartosc = wartosc.replace("\"", "{\n",1) + "\n}" #.replace("\"\"", "\"\n}",1)
    full_wartosc_list = wartosc.split("\n")
    wartosc_list = [entity_simple_in(el) for el in full_wartosc_list[1:-1]]
    wartosc = full_wartosc_list[0] + "\n" + ",\n".join(wartosc_list) + "\n" +full_wartosc_list[-1]
    if last:
        fout[0] += klucz+": "+wartosc+"\n"
    else:
        fout[0] += klucz+": "+wartosc+",\n"

def entity_list(fout, klucz, wartosc, last):
    #wartosc = wartosc.replace("\"\"", "[\n\"",1).replace("\"\"", "\"\n]",1)
    wartosc = wartosc.replace("\"", "",1)
    full_wartosc_list = wartosc.split("\n")
    wartosc_list = [entity_list_in(el) for el in full_wartosc_list[:]]
    #wartosc = full_wartosc_list[0] + "\n" + ",\n".join(wartosc_list) + "\n" +full_wartosc_list[-1]
    wartosc = "[\n"+",\n".join(wartosc_list)+"\n]"
    if last:
        fout[0] += klucz+": "+wartosc+"\n"
    else:
        fout[0] += klucz+": "+wartosc+",\n"

def entity_puzzled_list(fout, klucz, wartosc, last):
    wartosc = wartosc.replace("\"\"", "[\n\"",1).replace("\"\"", "\"\n]",1)
    full_wartosc_list = wartosc.split("\n")
    wartosc_list = [entity_list_in(el) for el in full_wartosc_list[1:-1]]
    wartosc = full_wartosc_list[0] + "\n" + ",\n".join(wartosc_list) + "\n" +full_wartosc_list[-1]
    if last:
        fout[0] += quote(klucz)+": "+wartosc+"\n"
    else:
        fout[0] += quote(klucz)+": "+wartosc+",\n"

def simple_list(fout, klucz, wartosc, last):
    #print("klucz", klucz, "wartosc", wartosc)
    wartosc = wartosc.replace("\"","")
    full_wartosc_list = wartosc.split(",")
    wartosc_list = [quote(el) for el in full_wartosc_list]
    wartosc = ",".join(wartosc_list)
    fout[0] += klucz+": ["+wartosc+"],\n"

def entity(fout, collected, last):
    ent_collected = collected[0:-2]
    if "," in ent_collected:
        klucz = ent_collected[0:ent_collected.index(",")]
        wartosc = ent_collected[ent_collected.index(",")+1:]

        if not "\n" in wartosc and not  "," in wartosc:
            entity_simple(fout, klucz, wartosc, last)

        if "\n" in wartosc and  ":" in wartosc and not "," in wartosc:
            entity_with_sub(fout, klucz, wartosc, last)

        if "\n" in wartosc and  ":" in wartosc and "," in wartosc:
            entity_list(fout, klucz, wartosc, last)

        if not "\n" in wartosc and  "," in wartosc:
            simple_list(fout, klucz, wartosc, last)

def convert_csv2_to_xml(csv_file, xml_file):
    try:

        fout = [""]
        with open(csv_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            fout[0] += "{\n"
            count = 0
            pc = ""
            step = 1
            collected = ""
            for line in lines:
                for c in line:
                    if c=="\"":
                        if pc == "\n":
                            step = 1
                        if pc == ",":
                            step = 1
                        if pc != "\n" and pc != "," and pc != "" and pc != "\"":
                            step = -1
                        count+=step
                    collected += c
                    pc = c
                if count == 2:
                    count = 0
                if count == 0:
                    last = False
                    if line==lines[-1]:
                        last = True
                    entity(fout, collected, last)
                    collected = ""
            fout[0] += "}\n"
            data = json.loads(fout[0])
            data["energySavingControl"] = None
            data["energySavingState"] = None
            data["energySavingStateNs3"] = None

            xml_bytes = dicttoxml(data, custom_root='config', attr_type=False)
            dom = parseString(xml_bytes)
            pretty_xml = dom.toprettyxml(indent="  ")
            with open(xml_file, "w") as f:
                f.write(pretty_xml)

        print(f"[INFO] Converted {csv_file} -> {xml_file}")
    except Exception as e:
        print(f"[ERROR] Failed to convert {csv_file}: {e}")



algebra = BooleanAlgebra()
def convert_csv1_to_xml(csv_file, xml_file):
    try:
        with open(csv_file, newline='', encoding='latin1') as f:
            csv_header = f.readline().strip()
            coma = False
            if csv_header.count(";") < csv_header.count(","):
                 coma = True
            if coma:
                if coma and csv_header.count(",") == 1:
                    convert_csv2_to_xml(csv_file, xml_file)
                    return
                else:
                    csv_header = csv_header.split(",")
            else:
                csv_header = csv_header.split(";")
            root = ET.Element("config")
            while True: 
                row = f.readline()
                if not row:
                    break
                if coma:
                    row = row.strip().split(",")
                else:
                    row = row.strip().split(";")

                entry = ET.SubElement(root, "entry")
                for header_index, header_key in enumerate(csv_header):
                    if header_key == "":
                        break
                    child = ET.SubElement(entry, header_key)
                    child.text = row[header_index]

            tree = ET.ElementTree(root)
            tree.write(xml_file)
            print(f"[INFO] Converted {csv_file} -> {xml_file}")
    except Exception as e:
        print(f"[ERROR] Failed to convert {csv_file}: {e}")
        
def convert_csv_to_xml(csv_file, xml_file):
    try:
        try:
            with open(csv_file, newline='', encoding='utf-8') as csvfile:
                reader = csv.DictReader(csvfile)
                rows = list(reader)
        except UnicodeDecodeError:
            with open(csv_file, newline='', encoding='latin1') as csvfile:
                reader = csv.DictReader(csvfile)
                rows = list(reader)

        root = ET.Element("config")
        for row in rows:
            entry = ET.SubElement(root, "entry")
            for key, val in row.items():
                child = ET.SubElement(entry, key)
                child.text = val

        tree = ET.ElementTree(root)
        tree.write(xml_file)
        print(f"[INFO] Converted {csv_file} -> {xml_file}")
    except Exception as e:
        print(f"[ERROR] Failed to convert {csv_file}: {e}")

def watch_csv_folder():
    seen_files = {}
    while True:
        for filename in os.listdir(CSV_DIR):
            if filename.endswith(".csv"):
                csv_path = os.path.join(CSV_DIR, filename)
                xml_path = os.path.join(XML_DIR, filename.replace(".csv", ".xml"))
                mtime = os.path.getmtime(csv_path)

                if filename not in seen_files or seen_files[filename] != mtime:
                    convert_csv1_to_xml(csv_path, xml_path)
                    seen_files[filename] = mtime

        time.sleep(1)

def compare(value, op, test_value):
    try:
        fval = float(value)
        ftest = float(test_value)
        if op == ">": return fval > ftest
        if op == "<": return fval < ftest
        if op == ">=": return fval >= ftest
        if op == "<=": return fval <= ftest
        return False
    except:
        return False

import re

def evaluate_expression_without_sp(expr, row):
    # Normalize operators and tokenize expression safely (even without spaces)
    expr = re.sub(r'(?<![=!<>])=(?!=)', '==', expr)  # fix bare '=' into '=='
    expr = re.sub(r'([()])', r' \1 ', expr)  # space around brackets
    expr = re.sub(r'([=!<>]=|[><]=?|=~)', r' \1 ', expr)  # space around operators
    expr = re.sub(r'\s+', ' ', expr).strip()  # normalize all spaces

    tokens = expr.split()
    rebuilt = []
    i = 0
    while i < len(tokens):
        if i + 2 < len(tokens) and tokens[i+1] in ("==", "!=", ">", "<", ">=", "<=", "=~"):
            key = tokens[i]
            op = tokens[i+1]
            val = tokens[i+2].strip('"').strip("'")
            actual = row.get(key, "")

            if op == "==":
                rebuilt.append(str(actual == val))
            elif op == "!=":
                rebuilt.append(str(actual != val))
            elif op in (">", "<", ">=", "<="):
                rebuilt.append(str(compare(actual, op, val)))
            elif op == "=~":
                try:
                    rebuilt.append(str(bool(re.search(val, actual))))
                except:
                    rebuilt.append("False")
            i += 3
        elif tokens[i].lower() in ("and", "or", "not", "(", ")"):
            rebuilt.append(tokens[i].lower())
            i += 1
        else:
            rebuilt.append(tokens[i])
            i += 1

    parsed = algebra.parse(" ".join(rebuilt))
    result = parsed.simplify()
    return bool(result)


def evaluate_expression(expr, row):
    # Replace all known operators and evaluate booleans
    tokens = expr.replace("(", " ( ").replace(")", " ) ").split()
    rebuilt = []
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        if i + 2 < len(tokens) and tokens[i+1] in ("==", "!=", ">", "<", ">=", "<=", "=~"):
            key = tokens[i]
            op = tokens[i+1]
            val = tokens[i+2].strip('"').strip("'")
            actual = row.get(key, "")

            if op == "==":
                rebuilt.append(str(actual == val))
            elif op == "!=":
                rebuilt.append(str(actual != val))
            elif op in (">", "<", ">=", "<="):
                rebuilt.append(str(compare(actual, op, val)))
            elif op == "=~":
                try:
                    rebuilt.append(str(bool(re.search(val, actual))))
                except:
                    rebuilt.append("False")
            i += 3
        elif tok.lower() in ("and", "or", "not", "(", ")"):
            rebuilt.append(tok.lower())
            i += 1
        else:
            rebuilt.append(tok)
            i += 1

    parsed = algebra.parse(" ".join(rebuilt))
    result = parsed.simplify()
    return bool(result)

import xml.etree.ElementTree as ET

def update_xml_parameters(xml_file, cell_name, params, parent_tag=None, output_file=None):
    """
    Update or add parameters in an XML file.

    Args:
        xml_file (str): Path to the XML file.
        params (dict): Dictionary of {tag: text_value}.
        parent_tag (str, optional): Tag name where new elements should be added.
                                    If None, use the root element.
        output_file (str, optional): Path to save updated XML. If None, overwrite input file.
    """
    tree = 0
    if os.path.exists(xml_file):

        tree = ET.parse(xml_file)
    else:
        with open(xml_file, "w") as file:
            file.write(f"<config><Name>{cell_name}</Name></config>")
        tree = ET.parse(xml_file)
    root = tree.getroot()
    
    parent = root if parent_tag is None else root.find(parent_tag)
    if parent is None:
        raise ValueError(f"Parent tag '{parent_tag}' not found in XML.")

    for tag, value in params.items():
        element = parent.find(tag)
        if element is not None:
            # Update existing
            element.text = str(value)
        else:
            # Add new
            new_elem = ET.Element(tag)
            new_elem.text = str(value)
            parent.append(new_elem)

    # Pretty print indentation (Python 3.9+)
    ET.indent(tree, space="  ")

    # Save file
    save_path = output_file if output_file else xml_file
    tree.write(save_path, encoding="utf-8", xml_declaration=True)


def getconfig():
   with open("config", "r") as f:
      return f.read()
   return "" 

def setconfig(file):
   with open("config", "w") as f:
       f.write(file)

def gnodeb_id_2_cell_name(gnodeb_id, relativeCellId):
    filename = getconfig()
    main_file = os.path.join(XML_DIR, filename)
    result = ""
    
    if os.path.exists(main_file):
        try:
            tree = ET.parse(main_file)
            config_root = tree.getroot()
            for entry in config_root.findall("entry"):
                row = {child.tag: child.text for child in entry}
                cell_name = row.get("cell_name")
                r_gnodeb_id = row.get("gnodeb_id")
                r_relativeCellId = row.get("RelativeCellId")
                if (r_gnodeb_id == gnodeb_id) and (r_relativeCellId == relativeCellId):
                    result = cell_name
        except Exception as e:
            print(f"[WARN] Failed to merge {e}")


    else:
        result = None
    return result

def merge_cell_config_into_CM(gnodeb_id, relativeCellId, energy_node):
    cell_name = gnodeb_id_2_cell_name(gnodeb_id, relativeCellId)
    if cell_name:
        print(cell_name)
        update_xml_parameters( f"xml/{cell_name}_CM.xml", cell_name, energy_node) #, parent_tag="config")
    else:
        raise Exception("Unknown cell")

def merge_cell_config(entry_node, cell_name, taglist=None):
    suffix_file = os.path.join(XML_DIR, f"{cell_name}_CM.xml")
    switch_file = os.path.join(XML_DIR, "energySavingControl.xml")
    if os.path.exists(suffix_file):
        try:
            tree = ET.parse(suffix_file)
            cell_config = tree.getroot()
            for child in cell_config:
                if taglist:
                    if child.tag in taglist:
                        entry_node.append(child)
                else:
                    entry_node.append(child)
   
#           switch_on = ET.fromstring("  <energySavingState>isNotEnergySaving</energySavingState>")
#           if os.path.exists(switch_file):
#               switch = ET.parse(switch_file)
#               cells = switch.getroot()
#               for cell in cells:
#                   if cell_name == cell.find("Name").text:
#                       switch_state = cell.find("energySavingControl")
#                       if switch_state is not None:
#                           switch_on = switch_state
#           entry_node.append(switch_on)
          
        except Exception as e:
            print(f"[WARN] Failed to merge {suffix_file}: {e}")

def onlytag(tag):
    if tag[0] == "{":
        _, _, tag = tag[1:].partition("}")
    return tag

def pretty_xml(xml_string: str) -> str:
    # Parse the XML string
    dom = xml.dom.minidom.parseString(xml_string)
    # Pretty print with indentation
    return dom.toprettyxml(indent="  ")

def pretty_xml_no_blanks(xml_string: str) -> str:
    dom = xml.dom.minidom.parseString(xml_string)

    # remove empty text nodes
    def remove_whitespace_nodes(node):
        for child in list(node.childNodes):
            if child.nodeType == child.TEXT_NODE and child.data.strip() == "":
                node.removeChild(child)
            elif child.hasChildNodes():
                remove_whitespace_nodes(child)

    remove_whitespace_nodes(dom)
    return dom.toprettyxml(indent="  ")

class RPCHandler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length).decode('utf-8')
        root = ET.fromstring(post_data)
        filename = root.findtext("filename")
        response = ""
        try:
            print(post_data)
            method = root.findtext("method")
            
            if method is None:
                message_id = 0
                if onlytag(root.tag) == "rpc":
                    message_id = root.get("message-id")
                method = onlytag(root[0].tag)
                print("method", method)
                if method == "edit-config":
                    idManagedElement = root.find(
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}edit-config/"\
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}config/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}ManagedElement/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}id"
                    ).text
                    idGNBCUCPFunction = root.findtext(
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}edit-config/"\
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}config/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}ManagedElement/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction}GNBCUCPFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction}id"
                    )
                    idNRCellCU = root.findtext(
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}edit-config/"\
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}config/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}ManagedElement/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction}GNBCUCPFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-nrcellcu}NRCellCU/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-nrcellcu}id"
                    )
                    idCESManagementFunction = root.findtext(
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}edit-config/"\
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}config/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}ManagedElement/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction}GNBCUCPFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-nrcellcu}NRCellCU/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}CESManagementFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}id"
                    )
                    energySavingControl = root.findtext(
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}edit-config/"\
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}config/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}ManagedElement/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction}GNBCUCPFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-nrcellcu}NRCellCU/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}CESManagementFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}attributes/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}energySavingControl"
                    )
                    energySavingState = root.findtext(
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}edit-config/"\
                    "{urn:ietf:params:xml:ns:netconf:base:1.0}config/"\
                    "{urn:3gpp:sa5:_3gpp-common-managed-element}ManagedElement/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction}GNBCUCPFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-nrcellcu}NRCellCU/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}CESManagementFunction/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}attributes/"\
                    "{urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction}energySavingState"
                    )
                    print("message_id",message_id)    
                    print("idManagedElement", idManagedElement)
                    print("idGNBCUCPFunction", idGNBCUCPFunction)
                    print("idNRCellCU", idNRCellCU)
                    print("idCESManagementFunction", idCESManagementFunction)
                    print("energySavingControl", energySavingControl)
                    print("energySavingState", energySavingState)

                    energy_node = {}
                    energy_node["message_id"] = message_id    
                    energy_node["idManagedElement"] = idManagedElement
                    energy_node["idGNBCUCPFunction"] = idGNBCUCPFunction
                    # energy_node["RelativeCellId"] = idNRCellCU
                    energy_node["idCESManagementFunction"] = idCESManagementFunction
                    energy_node["energySavingControl"] = energySavingControl
                    if energySavingControl == "toBeNotEnergySaving":
                        energy_node["energySavingState"] = "isNotEnergySaving"
                    elif energySavingControl == "toBeEnergySaving":
                        energy_node["energySavingState"] = "isEnergySaving"
                    gnodeb_id = idManagedElement
                    relativeCellId = idNRCellCU

                    merge_cell_config_into_CM(gnodeb_id, relativeCellId, energy_node)
                    
                    response = f"<rpc-reply message-id=\"{message_id}\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\
<ok/>\
</rpc-reply>"
                    print(response)
                if method == "init-config":
                    print("init-config*")
                    mainfile = root.find(
                    "init-config/config/file"
                    ).text.strip()
                    print(mainfile)
                    setconfig(mainfile)
                    response = f"<rpc-reply message-id=\"{message_id}\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\
                    <ok/>\
                    </rpc-reply>"
                if method == "get-config":
                    taglist = {"cell_name", "gnodeb_id", "RelativeCellId", "energySavingState"}
                    filter_expr = root.findtext("get-config/filter_expr")
                    filename = getconfig()
                    xml_path = os.path.join(XML_DIR, filename)
                    print(taglist, filter_expr, filename, xml_path)
                    try:
                        tree = ET.parse(xml_path)
                        config_root = tree.getroot()
                        if not filter_expr:
                            filter_expr = "true"
                        if filter_expr:
                            print ("Filter", filter_expr)
                            filtered = ET.Element("config")
                            for entry in config_root.findall("entry"):
                                row = {child.tag: child.text for child in entry}
                                if evaluate_expression_without_sp(filter_expr, row):
                                    cell_name = row.get("cell_name")
                                    if cell_name:
                                           new_entry = ET.Element("entry")
                                           for child in list(entry):
                                               if child.tag in taglist:
                                                   # Keep only tags in whitelist
                                                   new_entry.append(child)
                                           merge_cell_config(new_entry, cell_name, taglist)
 
                                    filtered.append(new_entry)
                            response = ET.tostring(filtered, encoding="unicode")
                        else:
                            response = ET.tostring(config_root, encoding="unicode")
                        response = f"<rpc-reply message-id=\"{message_id}\" xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">" + response + "</rpc-reply>"
                    except Exception as e:
                        response = f"<error>Could not read or filter file {filename}: {str(e)}</error>"

                
            # end if method is None
            elif method == "get_config" and filename:
                filter_expr = root.findtext("filter_expr")
                xml_path = os.path.join(XML_DIR, filename)
                try:
                    tree = ET.parse(xml_path)
                    config_root = tree.getroot()

                    if filter_expr:
                        print ("Filter", filter_expr)
                        filtered = ET.Element("config")
                        for entry in config_root.findall("entry"):
                            row = {child.tag: child.text for child in entry}
                            if evaluate_expression_without_sp(filter_expr, row):
                                cell_name = row.get("cell_name")
                                if cell_name:
                                    merge_cell_config(entry, cell_name)
 
                                filtered.append(entry)
                        response = ET.tostring(filtered, encoding="unicode")
                    else:
                        response = ET.tostring(config_root, encoding="unicode")
                except Exception as e:
                    response = f"<error>Could not read or filter file {filename}: {str(e)}</error>"
            elif method == "edit_config":
                config = root.findtext("config")
                try:

                    response = ET.tostring(config, encoding="unicode")
                except Exception as e:
                    response = f"<error>edit_config error: {str(e)}</error>"
                
                pass
            else:
                response = "<error>Missing method or filename</error>"
        except ET.ParseError as e:
            response = f"<error>XML parsing error: {str(e)}</error>"

        self.send_response(200)
        self.send_header("Content-type", "application/xml")
        self.end_headers()
        self.wfile.write(pretty_xml_no_blanks(response).encode("utf-8"))

if __name__ == "__main__":
    os.makedirs(CSV_DIR, exist_ok=True)
    os.makedirs(XML_DIR, exist_ok=True)

    watcher = Thread(target=watch_csv_folder, daemon=True)
    watcher.start()

    print(f"[RPC] Super filter server (>, <, =~) listening on port {PORT}")
    with socketserver.TCPServer(("", PORT), RPCHandler) as httpd:
        httpd.serve_forever()
