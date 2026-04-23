import threading
import asyncio
import json
import socket
import struct
from flask import Flask, request, jsonify, send_file
import aiocoap.resource as resource
import aiocoap

def obter_ip_local():
    """Descobre o IP real desta máquina na rede local"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

app = Flask(__name__)

# Variável global na memória
ultimo = {
    "flow": 0.0,
    "total": 0.0
}

# ==========================================
# 1. PARTE FLASK (HTTP / TCP - Porta 8000)
# ==========================================
@app.route('/')
def home():
    return send_file("index.html")

@app.route('/vazao', methods=['GET'])
def enviar():
    print(f"-> Flask recebeu um GET /vazao. Devolvendo: {ultimo}")
    
    resposta = jsonify(ultimo)
    
    # [CORREÇÃO] Força o navegador a NUNCA fazer cache desta rota
    resposta.headers["Cache-Control"] = "no-cache, no-store, must-revalidate"
    resposta.headers["Pragma"] = "no-cache"
    resposta.headers["Expires"] = "0"
    
    return resposta

# ==========================================
# 2. PARTE CoAP (UDP - Porta 5683)
# ==========================================
class VazaoResource(resource.Resource):
    async def render_put(self, req):
        global ultimo # [CORREÇÃO] Força a referência direta à variável do topo do arquivo
        
        try:
            flow, total = struct.unpack('<ff', req.payload)
            
            ultimo["flow"] = round(flow, 2)
            ultimo["total"] = round(total, 3)
            
            print(f"[{ultimo['flow']} L/min | Total: {ultimo['total']} L] CoAP Recebeu 8 bytes brutos!")
            
            return aiocoap.Message(code=aiocoap.CHANGED)
            
        except struct.error:
            print("Erro: O pacote recebido não tem 8 bytes ou está corrompido.")
            return aiocoap.Message(code=aiocoap.BAD_REQUEST)
        
def run_coap_server():
    async def main():
        root = resource.Site()
        root.add_resource(['vazao'], VazaoResource())
        
        meu_ip = obter_ip_local()
        print(f"Servidor CoAP escutando no IP exato: {meu_ip}:5683")
        
        await aiocoap.Context.create_server_context(root, bind=(meu_ip, 5683))
        await asyncio.get_running_loop().create_future()
        
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(main())


# ==========================================
# 3. INICIA TUDO
# ==========================================
if __name__ == '__main__':
    threading.Thread(target=run_coap_server, daemon=True).start()
    print("Iniciando servidores...")
    
    # Certifique-se de iniciar o script pelo terminal com: python seu_script.py
    app.run(host="0.0.0.0", port=8000, use_reloader=False, threaded=True)