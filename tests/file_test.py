from tests_lib import cmd
from tests_lib import settings
import tempfile
import os
import inspect
import hashlib
import random
import shutil
import logging
from multiprocessing import Process,Queue

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
CURR_DIR = os.path.abspath(currentdir)

GEN_FILE_COUNT = 10
PROC_NUM = 3
BLOCKSIZE = 128*hashlib.sha256().block_size #appropraite size for hashing large objects
TMP_FILE_MIN_SIZE = 10
TMP_FILE_MAX_SIZE = 10485760 #bytes in 10MB

IN_TMP_DIR = ""
OUT_TMP_DIR = ""
FILES_TO_HASH = Queue()

DS_CLIENT_PUT_CMD = settings.CLIENT_PATH + " put -s 0.0.0.0 -p 9111 -f "
DS_CLIENT_GET_CMD = settings.CLIENT_PATH + " get -s 0.0.0.0 -p 9111 -i " 
log = logging.getLogger('main')

def prepare():
	
	global IN_TMP_DIR 
	global OUT_TMP_DIR 
	
	IN_TMP_DIR = tempfile.mkdtemp(dir=CURR_DIR) + "/"
	OUT_TMP_DIR = tempfile.mkdtemp(dir=CURR_DIR) + "/"
	
	cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && ./start.sh", throw = True)

def cleanup():
	
	global IN_TMP_DIR
	global OUT_TMP_DIR
	
	shutil.rmtree(IN_TMP_DIR)
	shutil.rmtree(OUT_TMP_DIR)
	cmd.exec_cmd2("cd " + settings.PROJ_DIR + " && ./stop.sh", throw = True)

def gen_tmp_files(file_count):
	
	tmp_filenames=[];
	global IN_TMP_DIR 
	
	for f in range(0,file_count):
		tmp_file = tempfile.NamedTemporaryFile(prefix="",dir=IN_TMP_DIR,delete=False)
		random_data = os.urandom(random.randint(TMP_FILE_MIN_SIZE,TMP_FILE_MAX_SIZE))
		tmp_file.write(random_data)
		tmp_filenames.append(os.path.basename(tmp_file.name))
		tmp_file.close() 
	
	return tmp_filenames


def put_file(file_name):
	
	obj_id = cmd.exec_cmd2(DS_CLIENT_PUT_CMD + file_name, throw = True)
	
	return obj_id[1][0]


def get_file(out_file_name):
	
	cmd.exec_cmd2(DS_CLIENT_GET_CMD + out_file_name, throw = True)


def proc_routine(file_count):
	
	obj_ids = []
	global IN_TMP_DIR
	global OUT_TMP_DIR
	global FILES_TO_HASH

	file_names = gen_tmp_files(file_count)
	FILES_TO_HASH.put(file_names)

	for f in file_names:
		obj_ids.append(put_file(IN_TMP_DIR+f))
	
	for i in range(len(obj_ids)):
			get_file(obj_ids.pop()+" -f "+OUT_TMP_DIR+file_names[i])
			

def hash_large_file(path):
	
	global BLOCKSIZE
	hasher = hashlib.sha256()
	
	with open(path, "rb") as afile:
		buf = afile.read(BLOCKSIZE)
		while len(buf) > 0:
			hasher.update(buf)
			buf = afile.read(BLOCKSIZE)
	
	return buf


def check_file_hash(files_to_hash):
	
	global IN_TMP_DIR
	global OUT_TMP_DIR
	
	broken_files = []

	for filename in files_to_hash:
		
		path_to_in_file = IN_TMP_DIR + filename
		path_to_out_file = OUT_TMP_DIR + filename
		
		hash_in = hash_large_file(path_to_in_file)
		hash_out = hash_large_file(path_to_out_file)
		
		if hash_in != hash_out:
			broken_files.append(filename)

	if len(broken_files) != 0:
		log.error("FAILED: broken files: %s" % broken_files)
	else:
		log.info("PASSED: there are no broken files!")
	
	
			
if __name__ == "__main__":

	prepare()
	
	proc_list = [Process(target=proc_routine,args=(GEN_FILE_COUNT,)) for p in range(PROC_NUM)]
		
	for p in proc_list:
		p.start()
	
	for p in proc_list:
		p.join()
	
	for p in proc_list:
		log.info("%s (file put,get testing) exit with code: %d" % (p.name,p.exitcode))
	
	files_to_hash_list = [FILES_TO_HASH.get() for p in proc_list]

	checking_proc_list = [Process(target=check_file_hash,args=(files_to_hash_list.pop(),)) for p in proc_list]
	
	for p in checking_proc_list:
		p.start()
	
	for p in checking_proc_list:
		p.join()

	for p in checking_proc_list:
		log.info("%s (file's hash checking) exit with code: %d" % (p.name,p.exitcode))

	cleanup()

	