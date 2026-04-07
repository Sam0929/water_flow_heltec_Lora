from flask import Flask, request

app = Flask(__name__)

@app.route('/vazao', methods=['POST'])
def receber_vazao():

    data = request.json
    flow = data["flow"]
    total = data["total"]

    print("Vazao:", flow, "L/min")
    print("Total:", total, "L")

    return {"status": "ok"}

app.run(host="0.0.0.0", port=8000)