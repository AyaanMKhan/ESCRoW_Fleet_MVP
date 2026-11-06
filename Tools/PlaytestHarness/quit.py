import subprocess,time,sys
subprocess.run(['osascript','-e','tell application "UnrealEditor" to quit'],timeout=60)
t=time.time()
while time.time()-t<90:
    if subprocess.run(['pgrep','-f','UnrealEditor.app'],capture_output=True).returncode!=0: print('quit ok',round(time.time()-t)); sys.exit(0)
    time.sleep(3)
print('DID NOT QUIT'); sys.exit(4)
