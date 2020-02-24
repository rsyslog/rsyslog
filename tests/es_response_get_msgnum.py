import json
import os

with open(os.environ['RSYSLOG_DYNNAME'] + ".work") as json_file:
    json_data = json.load(json_file)
    json_data = json_data["hits"]
    for item in json_data["hits"]:
        print(item["_source"]["msgnum"])
