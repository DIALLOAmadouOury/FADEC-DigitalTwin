import customtkinter as ctk
import paho.mqtt.client as mqtt
import json
import csv
import random
from datetime import datetime

# ==========================================
# 1. CONFIGURATION DU FICHIER CSV
# ==========================================
csv_filename = "fadec_telemetry.csv"
with open(csv_filename, mode='a', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(["Heure", "RPM", "Temp(C)", "Fuel(kg/min)", "Prob_IA(%)", "Alerte"])

# ==========================================
# 2. LOGIQUE MQTT ET COMMANDES
# ==========================================
def on_connect(client, userdata, flags, rc, properties=None):
    print("[MQTT] Connecté au Broker HiveMQ avec succès !")
    client.subscribe("stm32/aod/fadec")
    app.after(0, lambda: conn_label.configure(text="🟢 DATA LINK: CONNECTED", text_color="#28A745"))

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        # Mise à jour UI
        app.after(0, update_ui, data)
        
        # Sauvegarde CSV
        with open(csv_filename, mode='a', newline='') as file:
            writer = csv.writer(file)
            writer.writerow([
                datetime.now().strftime("%H:%M:%S"), 
                round(data['rpm'], 1), round(data['tmp'], 1), 
                round(data['fuel'], 1), round(data['prob'] * 100, 1), data['warn']
            ])
    except Exception as e:
        print(f"Erreur MQTT : {e}")

def toggle_engine():
    global engine_running
    engine_running = not engine_running
    
    cmd = 1 if engine_running else 0
    client.publish("stm32/aod/command", json.dumps({"engine_cmd": cmd}))
    
    if engine_running:
        btn_engine.configure(text="🔴 ARRÊTER MOTEUR", fg_color="#DC3545", hover_color="#C82333")
    else:
        btn_engine.configure(text="🟢 DÉMARRER MOTEUR", fg_color="#28A745", hover_color="#218838")

def toggle_leak():
    global leak_active
    leak_active = not leak_active
    
    cmd = 1 if leak_active else 0
    client.publish("stm32/aod/command", json.dumps({"leak_cmd": cmd}))
    
    if leak_active:
        btn_leak.configure(text="🛠️ PURGER ET RÉPARER", fg_color="#FD7E14", hover_color="#E85E00")
        app.configure(fg_color="#FFF0F0") # Fond légèrement rouge pour l'alerte globale
    else:
        btn_leak.configure(text="🚨 SIMULER FUITE CRITIQUE 🚨", fg_color="#DC3545", hover_color="#C82333")
        app.configure(fg_color="#F5F7FA") # Retour au gris très clair

# ==========================================
# 3. INTERFACE GRAPHIQUE DIRECTE (LIGHT MODE)
# ==========================================
ctk.set_appearance_mode("Light") # MAGIE ICI : Passage en mode clair
ctk.set_default_color_theme("blue")

app = ctk.CTk()
app.geometry("850x650")
app.title("✈️ EICAS: Engine-Indicating and Crew-Alerting System")
app.configure(fg_color="#F5F7FA") # Fond gris très clair, parfait pour les caméras

leak_active = False
engine_running = False

def update_ui(data):
    # Lissage de l'affichage IA pour un effet pro (0% en temps normal)
    display_prob = 0.0 if data['prob'] < 0.45 else data['prob']

    # Textes (Couleurs foncées pour lisibilité sur fond blanc)
    rpm_label.configure(text=f"{data['rpm']:.0f} RPM")
    tmp_label.configure(text=f"{data['tmp']:.0f} °C")
    fuel_label.configure(text=f"{data['fuel']:.1f} kg/m")
    prob_label.configure(text=f"{display_prob*100:.1f} %")
    
    # Jauges
    rpm_bar.set(min(data['rpm'] / 10000.0, 1.0))
    tmp_bar.set(min(data['tmp'] / 1000.0, 1.0))
    fuel_bar.set(min(data['fuel'] / 50.0, 1.0))
    prob_bar.set(min(display_prob, 1.0))
    
    # Alarme (Gestion des couleurs d'alerte en Light Mode)
    if data['warn'] == 1:
        status_label.configure(text="⚠️ MASTER CAUTION: ENGINE LEAK ⚠️", text_color="#DC3545")
        frame_prob.configure(border_color="#DC3545", border_width=3)
        prob_bar.configure(progress_color="#DC3545")
    else:
        status_label.configure(text="✅ SYSTÈME MOTEUR NOMINAL", text_color="#28A745")
        frame_prob.configure(border_color="#E0E0E0", border_width=1)
        prob_bar.configure(progress_color="#28A745")

# --- Layout Grille ---
app.grid_columnconfigure(0, weight=1)
app.grid_columnconfigure(1, weight=1)

# Titre
ctk.CTkLabel(app, text="FADEC EDGE AI - TELEMETRY", font=("Trebuchet MS", 28, "bold"), text_color="#2C3E50").grid(row=0, column=0, columnspan=2, pady=(20, 5))
conn_label = ctk.CTkLabel(app, text="🟠 DATA LINK: SEARCHING...", font=("Consolas", 12, "bold"), text_color="#FD7E14")
conn_label.grid(row=1, column=0, columnspan=2, pady=(0, 20))

# Création des cartes blanches (Cards)
def create_gauge_card(parent, row, col, title, bar_color):
    frame = ctk.CTkFrame(parent, fg_color="#FFFFFF", border_color="#E0E0E0", border_width=1, corner_radius=15)
    frame.grid(row=row, column=col, padx=15, pady=10, sticky="nsew")
    ctk.CTkLabel(frame, text=title, font=("Arial", 12, "bold"), text_color="#7F8C8D").pack(pady=(15, 0))
    val_label = ctk.CTkLabel(frame, text="--", font=("Consolas", 36, "bold"), text_color=bar_color)
    val_label.pack(pady=(0, 5))
    bar = ctk.CTkProgressBar(frame, progress_color=bar_color, height=12, fg_color="#E9ECEF")
    bar.pack(pady=(5, 15), padx=20, fill="x")
    bar.set(0)
    return frame, val_label, bar

# Couleurs revues pour ressortir sur du blanc
_, rpm_label, rpm_bar = create_gauge_card(app, 2, 0, "N1 / VITESSE ROTATION", "#007BFF") # Bleu profond
_, tmp_label, tmp_bar = create_gauge_card(app, 2, 1, "EGT / TEMPÉRATURE GAZ", "#FF8C00") # Orange sombre
_, fuel_label, fuel_bar = create_gauge_card(app, 3, 0, "FF / DÉBIT CARBURANT", "#D4AC0D") # Jaune or

frame_prob, prob_label, prob_bar = create_gauge_card(app, 3, 1, "IA: DIAGNOSTIC D'ANOMALIE", "#28A745") # Vert par défaut

status_label = ctk.CTkLabel(app, text="SYS: STANDBY", font=("Arial", 20, "bold"), text_color="#6C757D")
status_label.grid(row=4, column=0, columnspan=2, pady=15)

# Boutons
btn_engine = ctk.CTkButton(app, text="🟢 DÉMARRER MOTEUR", font=("Arial", 16, "bold"), 
                           fg_color="#28A745", hover_color="#218838", text_color="#FFFFFF", height=50, corner_radius=10, command=toggle_engine)
btn_engine.grid(row=5, column=0, padx=(30, 10), pady=(0, 20), sticky="ew")

btn_leak = ctk.CTkButton(app, text="🚨 SIMULER FUITE CRITIQUE 🚨", font=("Arial", 16, "bold"), 
                         fg_color="#DC3545", hover_color="#C82333", text_color="#FFFFFF", height=50, corner_radius=10, command=toggle_leak)
btn_leak.grid(row=5, column=1, padx=(10, 30), pady=(0, 20), sticky="ew")

# ==========================================
# 4. LANCEMENT DU PROGRAMME
# ==========================================
client_id = f"FADEC_Cockpit_{random.randint(1000, 9999)}"
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id) # Correction du Warning Version 2 intégrée !
client.on_connect = on_connect
client.on_message = on_message

client.connect("broker.hivemq.com", 1883, 60)
client.loop_start()

app.mainloop()

client.loop_stop()
client.disconnect()