FROM alpine

ENV wd /app
WORKDIR ${wd}

COPY ./stompbroker.out ${wd}

EXPOSE 3490
CMD /app/stompbroker.out
