FROM alpine:3.9

COPY . /root/mcjoin
WORKDIR /root/mcjoin
RUN apk add --update build-base automake autoconf
RUN ./autogen.sh
RUN ./configure --prefix=/usr
RUN make

FROM alpine:3.9
COPY --from=0 /root/mcjoin/mcjoin /usr/bin/mcjoin

CMD [ "/usr/bin/mcjoin" ]
