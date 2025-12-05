import requests
import json
import pprint
import codecs

# URL of the server you want to send the XML to
url = "http://localhost:8830"

# XML string to send
xml_data2 = """<?xml version="1.0" encoding="UTF-8"?>
<rpc message-id="102">
  <init-config>
    <target><running/></target>
    <config>
      <file>
      Topo_Example.xml
      </file>
    </config>
  </init-config>
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

