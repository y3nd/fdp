import json
import subprocess
from time import sleep

ip = '10.43.3.148'

MIN_BASE_WINDOW_SIZE = 2
MAX_BASE_WINDOW_SIZE = 128

MIN_MAX_WINDOW_SIZE = 2
MAX_MAX_WINDOW_SIZE = 128

MIN_SLOWSTART_MULT = 2
MAX_SLOWSTART_MULT = 2

MIN_SLOWSTART_DIV = 2
MAX_SLOWSTART_DIV = 2


def main():
    with open('results.csv', 'a') as results:
        results.write('client_no,base_window_size,slowstart_mult,slowstart_div,speed,dataSent,dataReceived,time,segsSent,segsReceived,segsDropped,segsDropRate\n')

    for client_no in range(2, 2 + 1):
        for base_window_size in range(MIN_BASE_WINDOW_SIZE, MAX_BASE_WINDOW_SIZE + 1):
            for max_window_size in range(MIN_MAX_WINDOW_SIZE, MAX_MAX_WINDOW_SIZE + 1):
                for slowstart_mult in range(MIN_SLOWSTART_MULT, MAX_SLOWSTART_MULT + 1):
                    for slowstart_div in range(MIN_SLOWSTART_DIV, MAX_SLOWSTART_DIV + 1):
                        print("Compiling with")

                        c_code = f'''
                                #define BASE_WINDOW_SIZE {base_window_size}
                                #define MAX_WINDOW_SIZE {max_window_size}
                                #define SLOWSTART_MULT {slowstart_mult}
                                #define SLOWSTART_DIV {slowstart_div}'''

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
                            f"./client{client_no} {ip} 3000 random_5M.data 0",
                            shell=True,
                            text=True,
                            encoding="utf-8",
                            cwd="../bin"
                        )

                        client_p.wait()
                        sleep(0.1)
                        server_p.kill()
                        print('Server killed')

                        output = json.loads(server_p.communicate()[0])

                        with open('results.csv', 'a') as results:
                            results.write(f'{client_no},{base_window_size},{slowstart_mult},{slowstart_div},')
                            for i, key in enumerate(output):
                                s = f'{output[key]}'
                                if i != len(output) - 1:
                                    s += ','
                                results.write(s)
                            results.write('\n')


main()
