import json
import argparse
from collections import defaultdict

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='Archive and delete core app log files')
	parser.add_argument('--error', action='store', type=str, help='error')
	parser.add_argument('--response', action='store', type=str, help='response')
	args = parser.parse_args()

	messages = defaultdict(dict)
	with open(args.error, "r") as error_f, open(args.response, "r") as response_f:
		for line in error_f:
			json_obj = json.loads(line)
			# postdata contains a json string of records array
			records = json.loads(json_obj['request']['postdata'])
			if records:
				for i, val in enumerate(records['records']):
					messages[val['value']['msgnum']]['response'] = json_obj['response']
					messages[val['value']['msgnum']]['index'] = i
		#print (len(messages), "messages:", messages)

		# validate with responses
		for line in response_f:
			json_obj = json.loads(line)
			msgnum = json_obj['message']['msgnum']
			code = json_obj['response']['code']
			body = json_obj['response']['body']
			batch_index = json_obj['response']['batch_index']
			#print('msgnum:', msgnum, 'code:', code, 'body:', body, 'batch_index:', batch_index)
			assert(msgnum in messages)
			assert(messages[msgnum]['response']['status'] == code)
			assert(messages[msgnum]['response']['message'] == body)
			assert(messages[msgnum]['index'] == batch_index)
