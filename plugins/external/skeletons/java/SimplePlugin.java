/*

 Copyright (C) 2014 by Adiscon GmbH

This file is part of rsyslog.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.

You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
-or-
see COPYING.ASL20 in the source distribution

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/
import java.io.*;

public class SimplePlugin {
	
	public static void main(String[] args) throws IOException {
		try (BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
			BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream("out.txt", true)))) {
			String s;

			while ((s = in.readLine()) != null) {
				msgOut(s, writer);
			}
		}
	}

	public static void msgOut(String msg, BufferedWriter writerMsg) throws IOException {
		writerMsg.write(msg);
		writerMsg.newLine();
		writerMsg.flush();
	}

}
