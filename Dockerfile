FROM gcc:9.4.0-buster

COPY . /app

# RUN gcc /app/simul_cleaner.c -lrt -o mysimul_app 
RUN gcc /app/simul_three.c -lrt -o mysimul_app 

CMD bash