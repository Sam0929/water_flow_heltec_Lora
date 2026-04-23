import os
import requests
from dotenv import load_dotenv
from flask import Flask, render_template, jsonify

# Carrega as variáveis do arquivo .env
load_dotenv()

app = Flask(__name__)

THINGSBOARD_HOST = os.getenv("TB_HOST")
DEVICE_ID = os.getenv("TB_DEVICE_ID")
API_KEY = os.getenv("TB_API_KEY")

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/api/vazao')
def get_vazao():
    if not API_KEY or not DEVICE_ID:
        return jsonify({"error": "Configuração da API ausente no .env"}), 500

    # Endpoint para pegar as últimas telemetrias (flow e total)
    url = f"https://{THINGSBOARD_HOST}/api/plugins/telemetry/DEVICE/{DEVICE_ID}/values/timeseries?keys=flow,total"
    
    # O ThingsBoard exige o prefixo 'ApiKey ' antes da chave no cabeçalho
    headers = {
        "X-Authorization": f"ApiKey {API_KEY}"
    }

    try:
        response = requests.get(url, headers=headers)
        
        # Levanta um erro se o status HTTP não for 200 (ex: 401 Unauthorized)
        response.raise_for_status() 
        
        dados_brutos = response.json()

        # Extrai os valores do formato de série temporal do ThingsBoard
        flow = dados_brutos.get("flow", [{}])[0].get("value", 0)
        total = dados_brutos.get("total", [{}])[0].get("value", 0)

        return jsonify({
            "flow": float(flow),
            "total": float(total)
        })
        
    except requests.exceptions.HTTPError as err:
        return jsonify({"error": f"Erro na requisição ao ThingsBoard: {err}"}), response.status_code
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=8000, debug=True)