FROM gcc:9.4.0-buster
# FROM python:3.10.0-buster

COPY . /app

# RUN apt update
# RUN apt install -y vim

# RUN gcc /app/simul.c -lrt -o simul_og
RUN gcc /app/c_src/x2y.c -lrt -o x2y_out
# # RUN gcc /app/simul_cleaner.c -lrt -o mysimul_app 
# RUN gcc /app/simul_three.c -lrt -o mysimul_app 

# CMD [ "python3 /app/test_write.py | python /app/py_simul_three.py | python3 /app/print_event.py" ]
CMD bash
# CMD [ "bash", "/app/run-py-script.bash" ]

# to test `mysimul_app` manipulates specific input events given to it, run:
# python3 /app/test_write.py | ./mysimul_app | python3 /app/print_event.p
# python3 /app/test_write.py | python /app/py_simul_three.py | python3 /app/print_event.p
