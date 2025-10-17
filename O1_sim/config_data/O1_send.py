import requests
import json
import pprint
import codecs

# URL of the server you want to send the XML to
url = "http://localhost:8830"

# XML string to send
xml_data = """<?xml version="1.0" encoding="UTF-8"?>
<rpc>
    <method>edit-config</method>
</rpc>"""
xml_data2 = """<?xml version="1.0" encoding="UTF-8"?>
<rpc xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="102">
  <edit-config>
    <target><running/></target>
    <config>
      <ManagedElement xmlns="urn:3gpp:sa5:_3gpp-common-managed-element">
        <id>2625684</id>
        <GNBCUCPFunction xmlns="urn:3gpp:sa5:_3gpp-nr-nrm-gnbcucpfunction">
          <id>1</id>
          <NRCellCU xmlns="urn:3gpp:sa5:_3gpp-nr-nrm-nrcellcu">
            <id>5</id>
            <CESManagementFunction xmlns="urn:3gpp:sa5:_3gpp-nr-nrm-cesmanagementfunction">
              <id>5</id>
              <attributes>
                <energySavingControl>toBeEnergySaving</energySavingControl>
                <energySavingState>isNotEnergySaving</energySavingState>
              </attributes>
            </CESManagementFunction>
          </NRCellCU>
        </GNBCUCPFunction>
      </ManagedElement>
    </config>
  </edit-config>
</rpc>"""

# Headers to indicate that the body is XML
headers = {
    "Content-Type": "application/xml"
}

# Send the POST request
response = requests.post(url, data=xml_data2, headers=headers)

# Print the response
print("Status code:", response.status_code)
print("Response body:", response.text)


json_formatted_str = json.dumps(response.text, indent=2)

print(json_formatted_str)
pprint.pprint(json_formatted_str)

