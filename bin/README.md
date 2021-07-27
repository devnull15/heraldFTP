All binaries should go here.

Use the provided server binary to test against your client. It uses the protocols defined in Version_A_Candidate_Instructions and should communicate properly with your client.

Run it using ./example_server

Usage:
-p      port (Default: 4444)
-d      server home directory
-t      client session timeout (Default: 600 seconds)

Note: You MUST give it an absoulute path to use as a base directory. For you, this should be the absolute path of {path to project directory}/test/server. This will be where the server will serve from.