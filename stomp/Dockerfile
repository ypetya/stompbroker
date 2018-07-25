FROM gcc
WORKDIR /root
ADD . stomp/
EXPOSE 3490
RUN /root/stomp/build.sh && /root/stompbroker.out
