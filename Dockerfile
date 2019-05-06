FROM debian:stretch

COPY . /root/mcjoin
WORKDIR /root/mcjoin
RUN apt-get update && apt-get install -y build-essential automake
RUN ./autogen.sh
RUN ./configure --prefix=/usr
RUN make

FROM debian:stretch
COPY --from=0 /root/mcjoin/mcjoin /usr/bin/mcjoin

CMD [ "/usr/bin/mcjoin" ]
