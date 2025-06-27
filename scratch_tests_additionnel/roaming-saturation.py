#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import pandas as pd
import numpy as np
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import plotly.io as pio
from datetime import datetime
import jinja2
import sys
import argparse
import webbrowser

# --- CONFIGURATION ---
pd.set_option('display.max_columns', None)
pio.templates.default = "plotly_white"

def parse_command_line():
    """Parse les arguments de ligne de commande"""
    parser = argparse.ArgumentParser(description='Analyse des données de simulation WiFi roaming')
    parser.add_argument('-r', '--rssi', type=str, default='rssi_measurements.csv',
                        help='Chemin du fichier RSSI (default: rssi_measurements.csv)')
    parser.add_argument('-f', '--flow', type=str, default='flow_stats.csv',
                        help='Chemin du fichier flow stats (default: flow_stats.csv)')
    parser.add_argument('-H', '--handover', type=str, default='handover_events.csv',
                        help='Chemin du fichier handover events (default: handover_events.csv)')
    parser.add_argument('-o', '--output', type=str, default='wifi_analysis_report.html',
                        help='Chemin du fichier de sortie HTML (default: wifi_analysis_report.html)')
    parser.add_argument('--on', type=float, default=1.0, help="Durée ON (s) pour OnOffApplication")
    parser.add_argument('--off', type=float, default=0.0, help="Durée OFF (s) pour OnOffApplication")
    parser.add_argument('-s', '--show', action='store_true',
                        help='Ouvrir automatiquement le rapport dans un navigateur')
    return parser.parse_args()

def load_data(rssi_file, flow_file, handover_file):
    """Charge les données des fichiers CSV"""
    data = {}
    try:
        data['rssi'] = pd.read_csv(rssi_file) if os.path.exists(rssi_file) else None
        data['flow'] = pd.read_csv(flow_file) if os.path.exists(flow_file) else None
        data['handover'] = pd.read_csv(handover_file) if os.path.exists(handover_file) else None
    except Exception as e:
        print(f"Erreur lors du chargement des données: {e}")
        sys.exit(1)
    return data

def analyze_rssi(rssi_df, handover_df=None):
    """Analyse RSSI et graphique avec marqueurs handover"""
    graphs = {}
    stats = {}
    if rssi_df is None or len(rssi_df) == 0:
        return None, None

    stats['min_rssi'] = rssi_df['RSSI'].min()
    stats['max_rssi'] = rssi_df['RSSI'].max()
    stats['mean_rssi'] = rssi_df['RSSI'].mean()
    stats['n_stations'] = rssi_df['StationID'].nunique()
    stats['n_aps'] = rssi_df['APID'].nunique()
    stats['sim_duration'] = rssi_df['Time'].max()

    # RSSI au cours du temps + handover markers
    fig1 = make_subplots(rows=1, cols=1, subplot_titles=["RSSI au cours du temps"])
    for sta in rssi_df['StationID'].unique():
        for ap in rssi_df['APID'].unique():
            df_subset = rssi_df[(rssi_df['StationID'] == sta) & (rssi_df['APID'] == ap)]
            if len(df_subset) > 0:
                fig1.add_trace(
                    go.Scatter(
                        x=df_subset['Time'],
                        y=df_subset['RSSI'],
                        mode='lines',
                        name=f'STA{sta}→AP{ap}',
                        hovertemplate='T: %{x:.2f}s<br>RSSI: %{y:.2f} dBm'
                    )
                )
    # Ajout des markers handover (si handover_df fourni)
    if handover_df is not None and len(handover_df) > 0:
        hovers = handover_df[handover_df['EventType'] == 'HANDOVER']
        for _, row in hovers.iterrows():
            fig1.add_trace(
                go.Scatter(
                    x=[row['Time']],
                    y=[-70],  # On place le marker à -70 dBm (ou ajuster selon ton seuil)
                    mode='markers+text',
                    marker=dict(size=14, color='orange', symbol='star'),
                    text=['Handover'],
                    name=f'Handover STA{row["StationID"]}',
                    hovertemplate=f'Handover STA{row["StationID"]} à t={row["Time"]:.2f}s'
                )
            )
    fig1.update_layout(
        title="Évolution du RSSI au cours du temps (markers handover orange)",
        xaxis_title="Temps (s)",
        yaxis_title="RSSI (dBm)",
        hovermode="closest"
    )
    # Ligne seuil
    fig1.add_shape(type="line", x0=0, y0=-70, x1=rssi_df['Time'].max(), y1=-70,
                   line=dict(color="red", width=1, dash="dash"))
    fig1.add_annotation(x=rssi_df['Time'].max() * 0.02, y=-70,
                        text="Seuil typique (-70 dBm)", showarrow=False, yshift=10,
                        font=dict(size=10, color="red"))

    # Histogramme RSSI
    fig2 = px.histogram(
        rssi_df,
        x="RSSI",
        color="APID",
        title="Distribution du RSSI par AP",
        labels={"RSSI": "RSSI (dBm)", "count": "Nombre de mesures"},
        barmode="overlay", opacity=0.7
    )
    fig2.update_layout(xaxis_title="RSSI (dBm)", yaxis_title="Nombre de mesures", legend_title="APID")

    graphs['rssi_time'] = fig1
    graphs['rssi_histogram'] = fig2

    return graphs, stats

def analyze_handovers(handover_df):
    """Analyse handover et calcul du temps de handover (interruption)"""
    graphs = {}
    stats = {}
    if handover_df is None or len(handover_df) == 0:
        return None, None

    handovers = handover_df[handover_df['EventType'] == 'HANDOVER']
    stats['total_handovers'] = len(handovers)
    stats['stations_with_handovers'] = handovers['StationID'].nunique()
    stats['avg_handovers_per_station'] = len(handovers) / handovers['StationID'].nunique() if handovers['StationID'].nunique() else 0
    stats['first_handover_time'] = handovers['Time'].min() if len(handovers) > 0 else 0
    stats['last_handover_time'] = handovers['Time'].max() if len(handovers) > 0 else 0

    # Calcul du temps de handover (DEASSOC suivi de HANDOVER pour la même STA)
    interruptions = []
    for sta in handover_df['StationID'].unique():
        sta_events = handover_df[handover_df['StationID'] == sta].sort_values("Time")
        for i in range(1, len(sta_events)):
            if (sta_events.iloc[i]['EventType'] in ["ASSOC", "HANDOVER"] and
                sta_events.iloc[i-1]['EventType'] == "DEASSOC"):
                delta = sta_events.iloc[i]['Time'] - sta_events.iloc[i-1]['Time']
                interruptions.append(dict(StationID=sta, t1=sta_events.iloc[i-1]['Time'],
                                         t2=sta_events.iloc[i]['Time'], duration=delta))
    stats['mean_handover_interruption'] = np.mean([x['duration'] for x in interruptions]) if interruptions else 0
    stats['handover_interruptions'] = interruptions

    # Graphique chronologie
    fig = px.scatter(
        handover_df,
        x="Time",
        y="StationID",
        color="EventType",
        symbol="EventType",
        title="Chronologie des événements (handovers, assoc, deassoc)",
        labels={"Time": "Temps (s)", "StationID": "ID Station", "EventType": "Type"}
    )
    fig.update_layout(
        xaxis_title="Temps (s)",
        yaxis_title="ID de la station",
        legend_title="Type d'événement"
    )
    graphs['event_timeline'] = fig

    return graphs, stats

def analyze_flow(flow_df):
    """Analyse des stats de flux"""
    graphs = {}
    stats = {}
    if flow_df is None or len(flow_df) == 0:
        return None, None

    stats['total_flows'] = len(flow_df)
    stats['avg_throughput'] = flow_df['Throughput(Kbps)'].mean()
    stats['max_throughput'] = flow_df['Throughput(Kbps)'].max()
    stats['total_tx_packets'] = flow_df['TxPackets'].sum()
    stats['total_rx_packets'] = flow_df['RxPackets'].sum()
    stats['total_lost_packets'] = flow_df['LostPackets'].sum()
    stats['packet_loss_rate'] = (stats['total_lost_packets'] / stats['total_tx_packets'] * 100) if stats['total_tx_packets'] > 0 else 0

    # Débit par flux
    fig1 = px.bar(
        flow_df,
        x="FlowID",
        y="Throughput(Kbps)",
        color="Source",
        title="Débit par flux",
        labels={"FlowID": "ID du flux", "Throughput(Kbps)": "Débit (Kbps)", "Source": "Source"}
    )
    fig1.update_layout(xaxis_title="ID du flux", yaxis_title="Débit (Kbps)", legend_title="Source")

    # Pertes de paquets
    fig2 = go.Figure(go.Pie(
        labels=['Reçus', 'Perdus'],
        values=[stats['total_rx_packets'], stats['total_lost_packets']],
        textinfo='percent+value',
        hole=0.5,
        marker=dict(colors=['#3D9970', '#FF4136'])
    ))
    fig2.update_layout(
        title_text="Ratio des paquets reçus/perdus",
        annotations=[dict(text=f"Taux de perte: {stats['packet_loss_rate']:.2f}%", x=0.5, y=0.5, font_size=15, showarrow=False)]
    )

    # Délai moyen par flux
    fig3 = px.bar(
        flow_df,
        x="FlowID",
        y="DelaySum",
        color="Source",
        title="Délai cumulé par flux",
        labels={"FlowID": "ID du flux", "DelaySum": "Délai cumulé (s)", "Source": "Source"}
    )
    fig3.update_layout(xaxis_title="ID du flux", yaxis_title="Délai cumulé (s)", legend_title="Source")

    graphs['throughput'] = fig1
    graphs['packet_loss'] = fig2
    graphs['delay'] = fig3
    return graphs, stats

def create_html_report(data, graphs, stats, output_file, args):
    """Génère le rapport HTML (Jinja2) SANS les sections Handovers par station et Distribution du temps d'interruption"""
    template = """
    <!DOCTYPE html>
    <html lang="fr">
    <head>
        <meta charset="UTF-8">
        <title>Rapport d'analyse WiFi Roaming</title>
        <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
        <style>body { font-family: Arial; margin: 2em; }</style>
    </head>
    <body>
        <h1>Analyse Simulation WiFi Roaming</h1>
        <h2>Général</h2>
        <ul>
            <li><b>Date génération</b> : {{ generation_date }}</li>
            <li><b>Fichiers analysés</b> : {{ rssi_file }}, {{ handover_file }}, {{ flow_file }}</li>
            <li><b>Durée simulation</b> : {{ rssi_stats.sim_duration if rssi_stats else 'N/A' }} s</li>
            <li><b>Nombre STA</b> : {{ rssi_stats.n_stations if rssi_stats else 'N/A' }}</li>
            <li><b>Ratio ON/OFF</b> : ON={{ on_time }}s, OFF={{ off_time }}s ({{ on_ratio }}% ON)</li>
            <li><b>Handovers</b> : {{ handover_stats.total_handovers if handover_stats else 0 }}</li>
            <li><b>Moyenne interruption handover</b> : {{ handover_stats.mean_handover_interruption | round(4) if handover_stats else 'N/A' }} s</li>
            <li><b>Taux de perte</b> : {{ flow_stats.packet_loss_rate | round(2) if flow_stats else 0 }} %</li>
        </ul>
        <h2>RSSI au cours du temps (avec handovers)</h2>
        {{ rssi_graphs.rssi_time | safe }}
        <h2>Distribution RSSI</h2>
        {{ rssi_graphs.rssi_histogram | safe }}
        <h2>Handovers (chronologie)</h2>
        {{ handover_graphs.event_timeline | safe }}
        <h2>Débit par flux</h2>
        {{ flow_graphs.throughput | safe }}
        <h2>Ratio paquets reçus/perdus</h2>
        {{ flow_graphs.packet_loss | safe }}
        <h2>Délai cumulé par flux</h2>
        {{ flow_graphs.delay | safe }}
        <h2>Données brutes</h2>
        <pre>RSSI: {{ rssi_data.head(10).to_string() if rssi_data is not none else 'Aucune' }}</pre>
        <pre>HANDOVER: {{ handover_data.head(10).to_string() if handover_data is not none else 'Aucune' }}</pre>
        <pre>FLOW: {{ flow_data.head(10).to_string() if flow_data is not none else 'Aucune' }}</pre>
        <hr>
        <p>Rapport généré automatiquement.</p>
    </body>
    </html>
    """
    env = jinja2.Environment()
    # Ratio ON/OFF
    on_time = args.on
    off_time = args.off
    on_ratio = round(100 * on_time / (on_time + off_time), 1) if (on_time + off_time) > 0 else 100

    template_data = {
        'rssi_data': data.get('rssi'),
        'handover_data': data.get('handover'),
        'flow_data': data.get('flow'),
        'rssi_graphs': graphs.get('rssi'),
        'handover_graphs': graphs.get('handover'),
        'flow_graphs': graphs.get('flow'),
        'rssi_stats': stats.get('rssi'),
        'handover_stats': stats.get('handover'),
        'flow_stats': stats.get('flow'),
        'generation_date': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        'rssi_file': args.rssi,
        'handover_file': args.handover,
        'flow_file': args.flow,
        'on_time': on_time,
        'off_time': off_time,
        'on_ratio': on_ratio
    }
    from plotly.io import to_html
    # Remplace tous les objets Figure par leur version HTML
    for gtype in ['rssi', 'handover', 'flow']:
        if graphs.get(gtype):
            for k, fig in graphs[gtype].items():
                template_data[f"{gtype}_graphs"] = template_data.get(f"{gtype}_graphs", {})
                template_data[f"{gtype}_graphs"][k] = to_html(fig, include_plotlyjs=False, full_html=False)

    template_rendered = env.from_string(template).render(**template_data)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(template_rendered)
    print(f"✓ Rapport HTML généré: {output_file}")
    return output_file

def main():
    args = parse_command_line()
    print("\n=== Analyse des données de simulation WiFi ===\n")
    data = load_data(args.rssi, args.flow, args.handover)
    all_graphs, all_stats = {}, {}

    # Analyse RSSI
    if data.get('rssi') is not None:
        print("Analyse RSSI...")
        rssi_graphs, rssi_stats = analyze_rssi(data['rssi'], data.get('handover'))
        if rssi_graphs: all_graphs['rssi'] = rssi_graphs
        if rssi_stats: all_stats['rssi'] = rssi_stats

    # Analyse handover
    if data.get('handover') is not None:
        print("Analyse handover...")
        handover_graphs, handover_stats = analyze_handovers(data['handover'])
        if handover_graphs: all_graphs['handover'] = handover_graphs
        if handover_stats: all_stats['handover'] = handover_stats

    # Analyse flow
    if data.get('flow') is not None:
        print("Analyse flux...")
        flow_graphs, flow_stats = analyze_flow(data['flow'])
        if flow_graphs: all_graphs['flow'] = flow_graphs
        if flow_stats: all_stats['flow'] = flow_stats

    output_file = create_html_report(data, all_graphs, all_stats, args.output, args)
    if args.show:
        print(f"\nOuverture du rapport dans le navigateur...")
        webbrowser.open('file://' + os.path.realpath(output_file))

if __name__ == "__main__":
    main()
