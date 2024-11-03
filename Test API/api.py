from fastapi import FastAPI
from fastapi.responses import PlainTextResponse
import ssl

# creating API
app = FastAPI()

ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ssl_context.load_cert_chain('./cert.pem', keyfile='./key.pem')

@app.get("/api/test", response_class=PlainTextResponse)
async def getNumber():
    return "123|*456|789"
