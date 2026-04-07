from flask import Flask, request, jsonify, send_file

app = Flask(__name__)

ultimo = {
    "flow":0,
    "total":0
}

@app.route('/')
def home():
    return send_file("index.html")

@app.route('/vazao', methods=['POST'])
def receber():

    data = request.json

    ultimo["flow"] = data["flow"]
    ultimo["total"] = data["total"]

    return {"status":"ok"}

@app.route('/vazao', methods=['GET'])
def enviar():

    return jsonify(ultimo)


app.run(host="0.0.0.0", port=8000)