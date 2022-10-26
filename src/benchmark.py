import json
import subprocess
from time import sleep

ip = '134.214.202.220'

MIN_WINDOW_SIZE = 2
MAX_WINDOW_SIZE = 32

def main():
    with open('results.csv', 'w') as results:
        results.write('client_no,window_size,speed,dataSent,dataReceived,time,segsSent,segsReceived,segsDropped,segsDropRate,maxTimeout,minTimeout,maxWindow,minWindow\n')

    for client_no in range(2, 3):
        for window_size in range(MIN_WINDOW_SIZE, MAX_WINDOW_SIZE + 1):
            print("Compiling with")

            c_code = f'''
                #define BASE_WINDOW_SIZE {window_size}
                #define MAX_WINDOW_SIZE {window_size}
                #define SLOWSTART_MULT 1
                #define SLOWSTART_DIV 1'''

            with open('config.h', 'w') as config_h:
                config_h.write(c_code)

            print(c_code)

            subprocess.run(['make', 'clean'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            subprocess.run(['make'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

            print('Running server')

            server_p = subprocess.Popen(
                "./server 3000",
                stdout=subprocess.PIPE,
                shell=True,
                text=True,
                encoding="utf-8",
                cwd="../bin"
            )

            sleep(0.1)

            print(f'Running client{client_no}')

            client_p = subprocess.Popen(
                f"./client{client_no} {ip} 3000 random.data 0",
                shell=True,
                text=True,
                encoding="utf-8",
                cwd="../bin"
            )

            print("waiting for output on server..")
            outputstr = ""
            for i in range(1,15):
                outputstr += server_p.stdout.readline()
                 
            output = json.loads(outputstr)

            print(output)

            client_p.wait()
            print("CLIENT EXITED")
            sleep(0.1)
            
            server_p.kill()
            print('Server killed')

            

            print(output)

            with open('results.csv', 'a') as results:
                results.write(f'{client_no},{window_size},')
                for i, key in enumerate(output):
                    s = f'{output[key]}'
                    if i != len(output) - 1:
                        s += ','
                    results.write(s)
                results.write('\n')

            sleep(1)

        
        
                

main()
