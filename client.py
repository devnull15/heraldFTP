import socket
import struct
import argparse
import ipaddress
import select
import os
import sys

MAX_TCP_MSG = 4
MAX_PORT = 65535
MAX_MSG_SIZE = 2048

OPCODE = {
    "USER_OP":0x1,
    "DEL_OP":0x2,
    "LS_OP":0x3,
    "GET_OP":0x4,
    "MK_OP":0x5,
    "PUT_OP":0x6
}
RETCODE = {
    "SUCCESS":0x1,
    "SES_ERR":0x2,
    "PERM_ERR":0x3,
    "USR_EXIST":0x4,
    "FILE_EXIST":0x5,
    "FAIL":0xff
}

CLIENT_ROOT = os.getcwd() + "/test/client/"

# Global variable that will hold an instantiated Logins instance at run time
_logins = None



class Logins:
    """
    Class that keeps track of active sessions.
    
    This class is a dictionary that maps usernames to session IDs. In addition
    it keeps track of an active user and uses that session ID by default for operations

    """

    def __init__(self):
        self.logins = {}
        self.cur_user = None
        self.last_user = None
    
    def add_sesid(self, user, sesid):
        self.logins[user] = sesid
        
    def get_sesid(self, user):
        ret = 0
        try:
            ret = self.logins[user]
        except KeyError as e:
            print("[!] user not logged in %s" % e)
        return ret

    def get_curuser(self):
        ret = ''
        if self.cur_user is not None:
            ret = self.cur_user
        return ret

    def set_curuser(self, user):
        self.last_user = self.cur_user
        self.cur_user = user

    def login_fail(self):
        self.cur_user = self.last_user



def helpcmd(cmds):
    """
    Print available commands, optionally describes given command

    Args:
       cmds: the .split() command string minus the initial "help" command that has already been parsed
             if blank or invalid will just print available commands
    
    Returns:
        Nothing

    """
    options = ['get', 'put', 'help', 'quit', 'exit', 'delete', 'l_delete', 'ls', 'l_ls', 'mkdir', 'l_mkdir', 'user']
    availcmds = "[?] Available commands: " + str(options)
    
    if len(cmds) == 0:
        print(availcmds)
    elif cmds[0] == 'get':
        print("[?] get [src] [dst] - Gets a file from server [src] path and copies it into the client [dst] path")
    elif cmds[0] == 'put':
        print("[?] put [src] [dst] - Sends a file from client [src] path to be placed in the server [dst] path")
    elif cmds[0] == 'help':
        print("[?] help [opt: command] - Print available commands, optionally describes given command")
    elif cmds[0] == 'quit':
        print("[?] quit - Closes client")
    elif cmds[0] == 'exit':
        print("[?] exit - Closes client")
    elif cmds[0] == 'delete':
        print("[?] delete [path] - Deletes file at server [path]")
    elif cmds[0] == 'l_delete':
        print("[?] l_delete [path] - Deletes file at local [path], can't delete directories")
    elif cmds[0] == 'ls':
        print("[?] ls [optional path] - Lists remote directory contents")
    elif cmds[0] == 'l_ls':
        print("[?] l_ls [optional path] - Lists local directory contents")
    elif cmds[0] == 'mkdir':
        print("[?] mkdir [path] - Makes directory at server [path]")
    elif cmds[0] == 'l_mkdir':
        print("[?] l_mkdir [path] - Makes directory at client [path]")
    elif cmds[0] == 'user':
        useroptions = ['login', 'create-ro', 'create-rw', 'create-ad', 'delete']
        availusercmds = "[?] user %s - performs user authentication/creation functions" % useroptions
        if len(cmds) == 1:
            print(availusercmds)
        else:
            if cmds[1] == 'login':
                print("[?] user login [username] [password] - logs in with provided credentials")
            elif cmds[1] == 'create-ro':
                print("[?] user create-ro [username] [password] - creates read-only user with provided credentials")
            elif cmds[1] == 'create-rw':
                print("[?] user create-rw [username] [password] - creates read-write user with provided credentials")
            elif cmds[1] == 'create-ad':
                print("[?] user create-ad [username] [password] - creates admin user with provided credentials")
            elif cmds[1] == 'delete':
                print("[?] user delete [username] - deletes user")
            else:
                print("[?] invalid user command: %s" % cmds[1])
                print(availusercmds)
    else:
        print("[?] invalid command: %s" % cmds)
        print(availcmds)

    

    return


def usercmd(cmds):
    """
    Formats a message for the user operation functions of the server.

    The exact function performed depends on the second arguement, possible options are:
    1) login 2) create-ro 3) create-rw 4) create-ad 5) delete

    Args:
       cmds: the .split() command string minus the initial "user" command that has already been parsed
    
    Returns:
       bytearray object containing all the data for a request to the server, None on error

    """
    user_flag = {
        "login":0x00,
        "create-ro":0x01,
        "create-rw":0x02,
        "create-ad":0x03,
        "delete":0xff
    }

    global _logins
    
    if cmds[0] not in user_flag:
        cmds.insert(0,"user")
        helpcmd(cmds)
        return None

    ret = bytearray()
    ret.append(OPCODE["USER_OP"])
    ret.append(user_flag[cmds[0]])
    reserved = bytearray(b'\0\0')
    sesid = 0
    user = cmds[1]
    pswd = ''
    
    if len(cmds) == 3 and cmds[0] == 'login':
        pswd = cmds[2]
        _logins.set_curuser(user)
    elif len(cmds)  == 3 and 'create' in cmds[0]:
        pswd = cmds[2]
        sesid = _logins.get_sesid(_logins.get_curuser())
    elif len(cmds)  == 2 and cmds[0] == 'delete':
        sesid = _logins.get_sesid(_logins.get_curuser())
    else:
        cmds.insert(0,"user")
        helpcmd(cmds)
        return None


    ret += reserved
    ret += struct.pack('!H',len(user))
    ret += struct.pack('!H',len(pswd))
    ret += struct.pack('!L',sesid)
    usrpass = user + pswd
    ret += bytearray(usrpass, 'ascii')
        
    return ret

def getcmd(cmds):
    """
    Formats a message for the get operation functions of the server.

    gets a file from the server via the first arugment cmds[0] and copies it
    to the path specified in the second argument cmds[1]
    
    Args:
       cmds: the .split() command string minus the initial "get" command
       two arguments are required a server source path (cmds[0]) and a client
       destination path (cmds[1]). Both paths are relative to the root, the root
       directory for client is <project_root>/test/client/
    
    Returns:
       bytearray object containing all the data for a request to the server, None on error

    """
    global _logins
    
    if len(cmds) != 2:
        cmds.insert(0,"get")
        helpcmd(cmds)
        return None

    ret = bytearray()
    ret.append(OPCODE["GET_OP"])
    reserved = bytearray(b'\0')
    ret += reserved
    ret += struct.pack('!H',len(cmds[0]))
    sesid = _logins.get_sesid(_logins.get_curuser())
    ret += struct.pack('!L',sesid)
    ret += bytearray(cmds[0], 'ascii')

    return ret

def putcmd(cmds, overwrite):
    """
    Formats a message for the put operation functions of the server.

    gets a file from the client via the first arugment cmds[0] and sends it
    to the server at the specified path in the second argument cmds[1]
    
    Args:
       cmds: the .split() command string minus the initial "put" command
             two arguments are required a client source path (cmds[0]) and a server
             destination path (cmds[1]). Both paths are relative to the root, the root
             directory for client is <project_root>/test/client/
       overwrite: sets overwrite flag for the message authorizing server to overwrite
                  the file
    
    Returns:
       bytearray object containing all the data for a request to the server, None on error

    """
    global _logins
    
    if len(cmds) != 2:
        cmds.insert(0,"put")
        helpcmd(cmds)
        return None

    ret = bytearray()
    ret.append(OPCODE["PUT_OP"])
    ret.append(overwrite)
    ret += struct.pack('!H',len(cmds[1]))
    sesid = _logins.get_sesid(_logins.get_curuser())
    ret += struct.pack('!L',sesid)
    try:
        f = open(CLIENT_ROOT+cmds[0], "rb").read()
        print(f)
        ret += struct.pack('!L',len(f))
        ret += bytearray(cmds[1], 'ascii')
        ret += f
    except IOError as e:
        print("[!] put failed: %s" % e)
        ret = None
    return ret

def deletecmd(cmds):
    """
    Formats a message for the delete operation functions of the server.
    
    Args:
       cmds: the .split() command string minus the initial "delete" command
             one arguments is required the path and filename of the filed on
             the server that is to be deleted
    
    Returns:
       bytearray object containing all the data for a request to the server, None on error

    """
    global _logins
    
    if len(cmds) != 1:
        cmds.insert(0,"delete")
        helpcmd(cmds)
        return None

    ret = bytearray()
    ret.append(OPCODE["DEL_OP"])
    reserved = b'\0'
    ret += reserved
    ret += struct.pack('!H',len(cmds[0]))
    sesid = _logins.get_sesid(_logins.get_curuser())
    ret += struct.pack('!L',sesid)
    ret += bytearray(cmds[0], 'ascii')

    return ret

def lscmd(cmds, pos):
    """
    Lists files on remote server (optionally at a provided path)
    
    Args:
       cmds: the .split() command string minus the initial "ls" command
             cmds contains an optional path to the server directory to list
    
    Returns:
       bytearray object containing all the data for a request to the server, None on error

    """
    global _logins

    if len(cmds) > 1:
        cmds.insert(0,"ls")
        helpcmd(cmds)
        return None
    if len(cmds) == 0:
        cmds.append('.')
    ret = bytearray()
    ret.append(OPCODE["LS_OP"])
    reserved = b'\0'
    ret += reserved
    ret += struct.pack('!H',len(cmds[0]))
    sesid = _logins.get_sesid(_logins.get_curuser())
    ret += struct.pack('!L',sesid)
    ret += struct.pack('!L',pos)
    ret += bytearray(cmds[0], 'ascii')    

    return ret

def mkcmd(cmds, pos):
    """
    Makes directory on server=
    
    Args:
       cmds: the .split() command string minus the initial "mkdir" command
             cmds contains filename of directory to be created
    
    Returns:
        Nothing

    """
    global _logins

    if len(cmds) != 1:
        cmds.insert(0,"mkdir")
        helpcmd(cmds)
        return None

    ret = bytearray()
    ret.append(OPCODE["MK_OP"])
    reserved = b'\0'
    ret += reserved
    ret += struct.pack('!H',len(cmds[0]))
    sesid = _logins.get_sesid(_logins.get_curuser())
    ret += struct.pack('!L',sesid)
    ret += reserved*4
    ret += bytearray(cmds[0], 'ascii')    

    return ret
    
    


def parsecmd(cmds):
    """
    Parses a given command and formats a message to be sent to the server

    Args:
       cmds: list of string containing desired command to be sent along with arguments
    
    Returns:
       ascii encoded bytes object to be used by socket.recv(), None on error
    """
    ret = ''
    op = cmds[0]
    if op == 'user':
        ret = usercmd(cmds[1:])
    elif op == 'get':
        ret = getcmd(cmds[1:])
    elif op == 'put':
        ret = putcmd(cmds[1:], 0)
    elif op == 'delete':
        ret = deletecmd(cmds[1:])
    elif op == 'ls':
        ret = lscmd(cmds[1:], 0)
    elif op == 'mkdir':
        ret = mkcmd(cmds[1:], 0)
    else:
        print("[!] invalid command: " + op)
        ret = None
    return ret
    

def isvalidport(port):
    if not 0 < port < MAX_PORT:
        raise ValueError(f'{port} is not in valid range (0-65535)')

def getfile(cmds, recv):
    """
    Handles receiving the file from the server after a successful get command.

    Get will take a file from the server and write on the client at the path specified.
    This will overwrite files on the client without warning, except for this block of text,
    consider this your warning.

    Args:
       cmds: the .split() command string minus the initial "get" command
             two arguments are required a server source path (cmds[0]) and a client
             destination path (cmds[1]). Both paths are relative to the root, the root
             directory for client is <project_root>/test/client/
       recv: data received from the server containing the file content

    Returns:
       Nothing
    """
    len_offset = 2
    content_offset = 6
    content_len = int.from_bytes(recv[len_offset:content_offset], "big")
    content_end = content_offset + content_len
    content = recv[content_offset:content_end]
    ret = 0
    try:
        with open(CLIENT_ROOT+cmds[1], "wb") as f:
            ret += f.write(content)
        if ret < content_len:
            print("[!] get failed: file incomplete, ")
        else:
            print("[*] Getting file %s complete" % cmds[1])
    except IOError as e:
        print("[!] get failed: %s" % e)
    
    return

def overwritefile(cmds):
    """
    Overwrites file if user accepts.

    Prompts user if they want to overwrite file after a put command
    fails with FILE_EXISTS. If yes, will generate message to server
    with the overwrite flag set

    Args:
       cmds: the .split() command string
    
    Returns:
       bytearray object containing all the data for a request to the server
       or None if not overwriting

    """

    inp = ''
    ret = None
    while inp != 'y' and inp != 'n':
        inp = input("[!] Overwrite file? [y/N]").lower()
        if inp == 'y':
            print(cmds)
            ret = putcmd(cmds[1:], 1)
        elif inp == '':
            inp = 'n'
    return ret

def parsels(cmds, recv):
    """
    Parses the response from the ls command
    
    lists files and determines if additional mesages are required to
    receive all the data
    
    Args:
       cmds: the .split() command string
       recv: the data from the server
    
    Returns:
       bytearray object containing all the data for a request to the server
       or None if all data has been received

    """
    totallen = int.from_bytes(recv[4:8], "big")
    msglen = int.from_bytes(recv[8:12], "big")
    curpos = int.from_bytes(recv[12:16], "big")
    ret = None

    for i in range(msglen):
        p = i + 16
        if recv[p] == 1:
            sys.stdout.write('f: ')
        elif recv[p] == 2:
            sys.stdout.write('d: ')
        elif recv[p] == 0:
            sys.stdout.write('\n')
        else:
            sys.stdout.write(chr(recv[p]))
    if curpos != totallen:
        ret = lscmd(cmds[1:], curpos)
        
    return ret
    

def parserecv(cmds, recv):
    """
    Parses server response and performs appropriate actions based on the response

    Args:
       cmds: the .split() command string
       recv: data recevied by server
    
    Returns:
       bytearray object for a follow-up message to the server based on a previous command
       (like put overwrite) or None if no followup
    """
    global _logins
    ret = None
    if recv[0] == RETCODE["SUCCESS"]:
        print("[<] Success - Server action was successful")
        if cmds[0] == 'user' and cmds[1] == 'login':
            sesid = int.from_bytes(recv[2:6], "big")
            _logins.add_sesid(_logins.get_curuser(), sesid)
        if cmds[0] == 'get':
            getfile(cmds[1:], recv)
        if cmds[0] == 'ls':
            ret = parsels(cmds, recv)

    elif recv[0] == RETCODE["SES_ERR"]:
        print("[<] Session error - Provided Session ID was invalid or expired")
    elif recv[0] == RETCODE["PERM_ERR"]:
        print("""[<] Permission error - User associated with provided Session ID had
        insucient permissions to perform the action""")
    elif recv[0] == RETCODE["USR_EXIST"]:
        print("[<] User exists - User could not be created because it already exists")
    elif recv[0] == RETCODE["FILE_EXIST"]:
        print("[<] File exists - File could not be created because it already exists")
        if cmds[0] == 'put':
            ret = overwritefile(cmds) # BUGGED
    elif recv[0] == RETCODE["FAIL"]:
        print("[<] FAILURE - Server action failed")
        if cmds[0] == 'user' and cmds[1] == 'login':
            _logins.login_fail()
    else:
        print("[!] Unrecognized return code")
    return ret

def localdelete(path):
    """
    Deletes a file specified in the path.

    Args:
       path: path to file starting at client root
    
    Returns:
       Nothing
    """ 
    if len(path) < 1:
        print("[!] requires path")
        path.insert(0,'l_mkdir')
        helpcmd(path)
    targetdir = path[0]
    try:
        os.remove(CLIENT_ROOT+targetdir)
        print("[*] Directory %s deleted" % targetdir)
    except OSError as e:
        print("[!] delete failed (directories cannot be deleted by l_delete): %s" % e)
    return

def localmkdir(path):
    """
    Makes a directory with the given path.

    Args:
       path: path to directory to be made including name of the directory
       starts at client root
    
    Returns:
       Nothing
    """
    if len(path) < 1:
        print("[!] requires path")
        path.insert(0,'l_mkdir')
        helpcmd(path)
    targetdir = path[0]
    try:
        os.mkdir(CLIENT_ROOT+targetdir)
        print("[*] Directory %s created" % targetdir)
    except OSError as e:
        print("[!] mkdir failed: %s" % e)
    return

def localls(path):
    """
    Gives a listing of files for a directory on the local file system.

    Starts at client root by default, will optionally run on path if specified

    Args:
       path: optional variable to specify path to run file listing
    
    Returns:
       Nothing
    """
    
    targetdir = ''
    if len(path) > 0:
        targetdir += path[0]
    print("[*] %s" % '/'+targetdir)
    try:
        resultdir = CLIENT_ROOT+targetdir
        resultdir = os.path.normpath(resultdir)
        if len(CLIENT_ROOT) > len(resultdir):
            resultdir = CLIENT_ROOT
        ls = os.listdir(resultdir)
        for f in ls:
            print("[**] %s" % f)
    except IOError as e:
        print("[!] l_ls failed: %s" % e)
    return
    


def localcmd(cmds):
    """
    Handles commands that perform actions on the local filesystem

    Args:
       cmds: the .split() command string
    
    Returns:
       Nothing
    """
    options = {
        'l_delete':localdelete,
        'l_ls':localls,
        'l_mkdir':localmkdir,
    }

    if cmds[0] in options:
        options[cmds[0]](cmds[1:])
    else:
        print("here")
        helpcmd(cmds)
    return

def empty_socket(s):
    """
    Reades and discards all incoming data on a socket

    Args:
         s: connected to socket
    
    Returns:
       Nothing
    """
    inputsockets = [s]
    while 1:
        inp_ready, out_ready, err = select.select(inputsockets,[],[], 0.0)
        if len(inp_ready)==0:
            break
        inp_ready[0].recv(1)
    return

def main():
    global _logins
    _logins = Logins()
    clparser = argparse.ArgumentParser(description='client for capstone server')
    clparser.add_argument('ip', type=str, help='IP Address')
    clparser.add_argument('port', type=int, help='port number')
    args = vars(clparser.parse_args())
    ip = args['ip']
    port = args['port']

    try:
        ipaddress.ip_address(ip)
        isvalidport(port)
    except ValueError as e:
        print(e)
        return

       
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((ip,port))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    except ConnectionRefusedError as e:
        print(e)
        return
    while True:
        cmds = input("%s> " % _logins.get_curuser())
        empty_socket(s) # necessary to deal with I/O bug in example_server
        if cmds == '':
            continue
        
        cmds = cmds.split()
        op = cmds[0]
        
        if op == 'quit' or op == 'exit':
            break
        
        elif op == 'help':
            helpcmd(cmds.split()[1:])
                
        elif op[0:2] == "l_":
            localcmd(cmds)
                          
        else:
            followup = 'no'
            while followup is not None:
                if followup == 'no':
                    snd = parsecmd(cmds)
                else:
                    snd = followup
                if snd == None:
                    break
                print("[>] Sending: " + str(snd)) #DEBUG
                s.sendall(bytes(snd)) 
                recv = s.recv(MAX_MSG_SIZE)
                print("[<] Received: %s" % recv[:20]) #DEBUG
                if recv == b'':
                    print("[!] No data received from server")
                    followup = None
                else:
                    followup = parserecv(cmds, recv)
    print("[!] Quitting")
    #s.shutdown(socket.SHUT_RDWR)
    s.close()
    return
    
    

if __name__ == "__main__":
    main()
