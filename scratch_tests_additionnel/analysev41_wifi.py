#!/usr/bin/env python3

import pandas as pd
import numpy as np
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import os
import argparse
import jinja2
import sys
from datetime import datetime

def load_data(rssi_file='rssi_measurements.csv', handover_file='handover_events.csv', flow_file='flow_stats.csv'):
    """Charge les données des fichiers CSV"""
    data = {}
    
    # Charger les mesures RSSI
    if os.path.exists(rssi_file):
        try:
            data['rssi'] = pd.read_csv(rssi_file)
            print(f"✓ Chargement de {rssi_file}: {len(data['rssi'])} mesures RSSI")
        except Exception as e:
            print(f"❌ Erreur lors du chargement de {rssi_file}: {e}")
    else:
        print(f"❌ Fichier {rssi_file} non trouvé")
        data['rssi'] = None
    
    # Charger les statistiques de flux
    if os.path.exists(flow_file):
        try:
            data['flow'] = pd.read_csv(flow_file)
            print(f"✓ Chargement de {flow_file}: {len(data['flow'])} enregistrements de flux")
        except Exception as e:
            print(f"❌ Erreur lors du chargement de {flow_file}: {e}")
    else:
        print(f"❌ Fichier {flow_file} non trouvé")
        data['flow'] = None
    
    # Charger les événements de handover
    if os.path.exists(handover_file):
        try:
            data['handover'] = pd.read_csv(handover_file)
            print(f"✓ Chargement de {handover_file}: {len(data['handover'])} événements")
        except Exception as e:
            print(f"❌ Erreur lors du chargement de {handover_file}: {e}")
    else:
        print(f"❌ Fichier {handover_file} non trouvé")
        data['handover'] = None
    
    return data

def analyze_rssi(data):
    """Analyse les données RSSI et génère des visualisations"""
    if data['rssi'] is None:
        return None, None
    
    print("\nAnalyse des données RSSI...")
    rssi_df = data['rssi']
    
    # Calculs statistiques sur le RSSI
    rssi_stats = rssi_df.groupby(['StationID', 'APID'])['RSSI'].agg(['mean', 'min', 'max']).reset_index()
    rssi_stats.columns = ['StationID', 'APID', 'Mean RSSI', 'Min RSSI', 'Max RSSI']
    
    # Graphiques
    graphs = {}
    
    # Graphique 1: Évolution du RSSI pour chaque station vers chaque AP
    fig_rssi_evolution = make_subplots(rows=min(5, len(rssi_df['StationID'].unique())), 
                                       cols=1, 
                                       subplot_titles=[f"Station {i}" for i in range(min(5, len(rssi_df['StationID'].unique())))])
    
    for i, station_id in enumerate(sorted(rssi_df['StationID'].unique())[:5]):  # Limiter à 5 stations pour la lisibilité
        station_data = rssi_df[rssi_df['StationID'] == station_id]
        
        for ap_id in sorted(station_data['APID'].unique()):
            ap_data = station_data[station_data['APID'] == ap_id]
            fig_rssi_evolution.add_trace(
                go.Scatter(x=ap_data['Time'], y=ap_data['RSSI'], mode='lines',
                           name=f"AP{ap_id}", line=dict(width=2)),
                row=i+1, col=1
            )
        
        # Ajouter une ligne horizontale pour le seuil à -70dBm
        fig_rssi_evolution.add_hline(y=-70, line=dict(color='red', width=1, dash='dash'),
                                     annotation_text="Seuil typique (-70dBm)",
                                     annotation_position="top right",
                                     row=i+1, col=1)
    
    fig_rssi_evolution.update_layout(
        height=300*min(5, len(rssi_df['StationID'].unique())),
        title="Évolution du RSSI dans le temps",
        xaxis_title="Temps (s)",
        yaxis_title="RSSI (dBm)",
        legend_title="Point d'accès",
        template="plotly_white"
    )
    
    # Ajuster les superpositions de légende
    for i in range(min(5, len(rssi_df['StationID'].unique()))):
        fig_rssi_evolution.update_xaxes(title_text="Temps (s)", row=i+1, col=1)
        fig_rssi_evolution.update_yaxes(title_text="RSSI (dBm)", row=i+1, col=1)
    
    graphs['rssi_evolution'] = fig_rssi_evolution
    
    # Graphique 2: Heatmap RSSI par position
    # On le fait uniquement pour la station 0 par simplicité
    if 0 in rssi_df['StationID'].values:
        station0_data = rssi_df[rssi_df['StationID'] == 0].copy()
        
        # Créer une grille pour la heatmap
        station0_data['PosX_grid'] = pd.cut(station0_data['PosX'], bins=20, labels=False)
        station0_data['PosY_grid'] = pd.cut(station0_data['PosY'], bins=10, labels=False)
        
        # Agréger les valeurs RSSI par cellule de grille
        heatmap_data = station0_data.groupby(['PosX_grid', 'PosY_grid', 'APID'])['RSSI'].mean().reset_index()
        
        # Créer un graphique pour chaque AP
        fig_rssi_heatmap = make_subplots(rows=len(heatmap_data['APID'].unique()), 
                                          cols=1, 
                                          subplot_titles=[f"AP {ap}" for ap in sorted(heatmap_data['APID'].unique())])
        
        for i, ap in enumerate(sorted(heatmap_data['APID'].unique())):
            ap_heatmap = heatmap_data[heatmap_data['APID'] == ap].pivot(index='PosY_grid', columns='PosX_grid', values='RSSI')
            
            fig_rssi_heatmap.add_trace(
                go.Heatmap(z=ap_heatmap.values,
                           x=ap_heatmap.columns,
                           y=ap_heatmap.index,
                           colorscale='Viridis',
                           colorbar=dict(title='RSSI (dBm)'),
                           zmin=-90, zmax=-30),
                row=i+1, col=1
            )
        
        fig_rssi_heatmap.update_layout(
            height=300*len(heatmap_data['APID'].unique()),
            title="Carte de chaleur RSSI pour la Station 0",
            template="plotly_white"
        )
        
        for i in range(len(heatmap_data['APID'].unique())):
            fig_rssi_heatmap.update_xaxes(title_text="Position X (grille)", row=i+1, col=1)
            fig_rssi_heatmap.update_yaxes(title_text="Position Y (grille)", row=i+1, col=1)
        
        graphs['rssi_heatmap'] = fig_rssi_heatmap
    
    # Graphique 3: Distribution du RSSI
    fig_rssi_dist = go.Figure()
    
    for ap_id in sorted(rssi_df['APID'].unique()):
        ap_data = rssi_df[rssi_df['APID'] == ap_id]
        fig_rssi_dist.add_trace(
            go.Histogram(x=ap_data['RSSI'], name=f"AP{ap_id}",
                         opacity=0.7, nbinsx=30)
        )
    
    fig_rssi_dist.update_layout(
        barmode='overlay',
        title="Distribution des valeurs RSSI",
        xaxis_title="RSSI (dBm)",
        yaxis_title="Fréquence",
        legend_title="Point d'accès",
        template="plotly_white"
    )
    
    # Ajouter une ligne verticale pour le seuil à -70dBm
    fig_rssi_dist.add_vline(x=-70, line=dict(color='red', width=2, dash='dash'),
                           annotation=dict(text="Seuil typique", showarrow=False, xshift=10))
    
    graphs['rssi_distribution'] = fig_rssi_dist
    
    return graphs, rssi_stats

def analyze_handovers(data):
    """Analyse les handovers et génère des visualisations"""
    if data['handover'] is None:
        return None, None
    
    print("\nAnalyse des événements de handover...")
    handover_df = data['handover']
    
    # Filtrer pour obtenir uniquement les événements HANDOVER
    handovers = handover_df[handover_df['EventType'] == 'HANDOVER']
    
    # Statistiques des handovers
    handover_stats = {
        'total_handovers': len(handovers),
        'handovers_by_station': handovers['StationID'].value_counts().to_dict()
    }
    
    # Convertir en DataFrame pour l'affichage
    handover_stats_df = pd.DataFrame([
        {'Métrique': 'Nombre total de handovers', 'Valeur': handover_stats['total_handovers']},
        {'Métrique': 'Temps moyen entre handovers (s)', 'Valeur': (handover_df['Time'].max() - handover_df['Time'].min()) / max(1, len(handovers))}
    ])
    
    # Graphiques
    graphs = {}
    
    # Graphique 1: Chronologie des handovers
    fig_timeline = go.Figure()
    
    # Tracer les événements d'association
    assoc_events = handover_df[handover_df['EventType'] == 'ASSOC']
    for i, row in assoc_events.iterrows():
        fig_timeline.add_trace(go.Scatter(
            x=[row['Time']], y=[row['StationID']],
            mode='markers',
            marker=dict(symbol='circle', size=10, color='blue'),
            name='Association',
            hovertemplate='Association à %{x:.2f}s<br>Station %{y}'
        ))
    
    # Tracer les événements de désassociation
    deassoc_events = handover_df[handover_df['EventType'] == 'DEASSOC']
    for i, row in deassoc_events.iterrows():
        fig_timeline.add_trace(go.Scatter(
            x=[row['Time']], y=[row['StationID']],
            mode='markers',
            marker=dict(symbol='x', size=10, color='red'),
            name='Désassociation',
            hovertemplate='Désassociation à %{x:.2f}s<br>Station %{y}'
        ))
    
    # Tracer les événements de handover
    for i, row in handovers.iterrows():
        fig_timeline.add_trace(go.Scatter(
            x=[row['Time']], y=[row['StationID']],
            mode='markers',
            marker=dict(symbol='star', size=15, color='green'),
            name='Handover',
            hovertemplate='Handover à %{x:.2f}s<br>Station %{y}<br>De %{text} vers AP %{customdata}',
            text=[row['AccessPoint1']],
            customdata=[row['AccessPoint2']]
        ))
    
    # Astuce pour avoir une seule entrée par type d'événement dans la légende
    names = set()
    fig_timeline.for_each_trace(
        lambda trace: trace.update(showlegend=False) 
        if (trace.name in names) else names.add(trace.name)
    )
    
    fig_timeline.update_layout(
        title="Chronologie des événements",
        xaxis_title="Temps (s)",
        yaxis_title="Station ID",
        legend_title="Type d'événement",
        template="plotly_white"
    )
    
    # Assurer que l'axe Y montre des entiers pour les IDs de station
    fig_timeline.update_yaxes(
        tickmode='linear',
        tick0=0,
        dtick=1
    )
    
    graphs['handover_timeline'] = fig_timeline
    
    # Graphique 2: Distribution des handovers par station
    if len(handover_stats['handovers_by_station']) > 0:
        handovers_by_station_df = pd.DataFrame({
            'StationID': list(handover_stats['handovers_by_station'].keys()),
            'Nombre de handovers': list(handover_stats['handovers_by_station'].values())
        })
        
        fig_handovers_by_station = px.bar(
            handovers_by_station_df,
            x='StationID',
            y='Nombre de handovers',
            title="Nombre de handovers par station",
            labels={'StationID': 'ID de la station', 'Nombre de handovers': 'Nombre de handovers'},
            template="plotly_white"
        )
        
        # Assurer que l'axe X montre des entiers pour les IDs de station
        fig_handovers_by_station.update_xaxes(
            tickmode='linear',
            tick0=0,
            dtick=1
        )
        
        graphs['handovers_by_station'] = fig_handovers_by_station
    
    # Graphique 3: Carte des positions lors des handovers (si données RSSI disponibles)
    if data['rssi'] is not None:
        # Trouver les positions au moment des handovers
        handover_positions = []
        rssi_df = data['rssi']
        
        for i, row in handovers.iterrows():
            station_id = row['StationID']
            handover_time = row['Time']
            
            # Trouver la position la plus proche du temps de handover
            station_positions = rssi_df[rssi_df['StationID'] == station_id]
            if not station_positions.empty:
                closest_idx = (station_positions['Time'] - handover_time).abs().idxmin()
                position = station_positions.loc[closest_idx]
                
                handover_positions.append({
                    'Time': handover_time,
                    'StationID': station_id,
                    'PosX': position['PosX'],
                    'PosY': position['PosY'],
                    'FromAP': row['AccessPoint1'],
                    'ToAP': row['AccessPoint2']
                })
        
        if handover_positions:
            handover_pos_df = pd.DataFrame(handover_positions)
            
            fig_handover_map = px.scatter(
                handover_pos_df,
                x='PosX',
                y='PosY',
                color='StationID',
                hover_data=['Time', 'FromAP', 'ToAP'],
                labels={'PosX': 'Position X (m)', 'PosY': 'Position Y (m)'},
                title="Position des stations lors des handovers",
                template="plotly_white"
            )
            
            graphs['handover_map'] = fig_handover_map
    
    return graphs, handover_stats_df

def analyze_flows(data):
    """Analyse les statistiques de flux et génère des visualisations"""
    if data['flow'] is None:
        return None, None
    
    print("\nAnalyse des statistiques de flux...")
    flow_df = data['flow']
    
    # Calculer des métriques supplémentaires si nécessaires
    if 'LossRate' not in flow_df.columns and 'TxPackets' in flow_df.columns and 'LostPackets' in flow_df.columns:
        flow_df['LossRate'] = flow_df['LostPackets'] / flow_df['TxPackets'].replace(0, 1) * 100
        
    if 'Throughput' not in flow_df.columns and 'RxBytes' in flow_df.columns and 'Duration' in flow_df.columns:
        flow_df['Throughput'] = flow_df['RxBytes'] * 8 / flow_df['Duration'].replace(0, 1) / 1000  # kbps
    
    # Graphiques
    graphs = {}
    
    # Graphique 1: Débit par flux
    if 'Throughput' in flow_df.columns:
        fig_throughput = px.bar(
            flow_df,
            x='FlowID',
            y='Throughput',
            title="Débit par flux",
            labels={'FlowID': 'ID du flux', 'Throughput': 'Débit (kbps)'},
            template="plotly_white"
        )
        
        graphs['throughput'] = fig_throughput
    
    # Graphique 2: Taux de perte par flux
    if 'LossRate' in flow_df.columns:
        fig_loss = px.bar(
            flow_df,
            x='FlowID',
            y='LossRate',
            title="Taux de perte par flux",
            labels={'FlowID': 'ID du flux', 'LossRate': 'Taux de perte (%)'},
            template="plotly_white"
        )
        
        graphs['packet_loss'] = fig_loss
    
    # Graphique 3: Délai moyen par flux
    if 'MeanDelay' in flow_df.columns:
        fig_delay = px.bar(
            flow_df,
            x='FlowID',
            y='MeanDelay',
            title="Délai moyen par flux",
            labels={'FlowID': 'ID du flux', 'MeanDelay': 'Délai moyen (s)'},
            template="plotly_white"
        )
        
        graphs['delay'] = fig_delay
    
    # Statistiques sommaires
    flow_stats = {
        'total_flows': len(flow_df),
        'avg_throughput': flow_df['Throughput'].mean() if 'Throughput' in flow_df.columns else None,
        'avg_loss_rate': flow_df['LossRate'].mean() if 'LossRate' in flow_df.columns else None,
        'avg_delay': flow_df['MeanDelay'].mean() if 'MeanDelay' in flow_df.columns else None
    }
    
    # Convertir en DataFrame pour l'affichage
    flow_stats_df = pd.DataFrame([
        {'Métrique': 'Nombre total de flux', 'Valeur': flow_stats['total_flows']},
        {'Métrique': 'Débit moyen (kbps)', 'Valeur': flow_stats['avg_throughput']},
        {'Métrique': 'Taux de perte moyen (%)', 'Valeur': flow_stats['avg_loss_rate']},
        {'Métrique': 'Délai moyen (s)', 'Valeur': flow_stats['avg_delay']}
    ])
    
    return graphs, flow_stats_df

def create_html_report(data, all_graphs, all_stats, output_file, rssi_file='rssi_measurements.csv', handover_file='handover_events.csv', flow_file='flow_stats.csv'):
    """Crée un rapport HTML avec tous les graphiques"""
    
    # Template Jinja2 pour le rapport HTML
    template_str = """<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Rapport d'analyse WiFi Roaming</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body {
            font-family: Arial, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }
        h1, h2, h3 {
            color: #2c5282;
        }
        h1 {
            text-align: center;
            border-bottom: 2px solid #2c5282;
            padding-bottom: 10px;
        }
        .section {
            margin-bottom: 40px;
            padding: 20px;
            background-color: #f8f9fa;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .plot {
            width: 100%;
            height: auto;
            margin: 20px 0;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        th, td {
            border: 1px solid #ddd;
            padding: 8px 12px;
            text-align: left;
        }
        th {
            background-color: #f2f2f2;
        }
        tr:nth-child(even) {
            background-color: #f9f9f9;
        }
        .footer {
            margin-top: 50px;
            text-align: center;
            font-size: 0.9em;
            color: #666;
        }
        .summary-box {
            background-color: #e6f7ff;
            border-left: 4px solid #1890ff;
            padding: 15px;
            margin: 20px 0;
        }
        .warning {
            background-color: #fff7e6;
            border-left: 4px solid #ffa940;
        }
        .success {
            background-color: #f6ffed;
            border-left: 4px solid #52c41a;
        }
    </style>
</head>
<body>
    <h1>Rapport d'analyse WiFi Roaming</h1>
    
    <div class="section">
        <h2>Résumé</h2>
        <p>Ce rapport présente l'analyse de la simulation de roaming WiFi avec les données suivantes:</p>
        <ul>
            <li>Fichier RSSI: {{ rssi_file }}</li>
            <li>Fichier Handover: {{ handover_file }}</li>
            <li>Fichier Flow: {{ flow_file }}</li>
        </ul>
        
        <div class="summary-box">
            <h3>Points clés</h3>
            <ul>
                {% if has_rssi %}
                <li>Nombre de mesures RSSI: {{ rssi_count }}</li>
                {% endif %}
                
                {% if has_handover %}
                <li>Nombre total de handovers: {{ handover_count }}</li>
                {% endif %}
                
                {% if has_flow %}
                <li>Nombre de flux analysés: {{ flow_count }}</li>
                {% if avg_throughput %}
                <li>Débit moyen: {{ avg_throughput|round(2) }} kbps</li>
                {% endif %}
                {% endif %}
            </ul>
        </div>
    </div>
    
    {% if has_rssi %}
    <div class="section">
        <h2>Analyse RSSI</h2>
        <p>Cette section présente l'analyse de la puissance du signal reçu (RSSI) au cours de la simulation.</p>
        
        <h3>Évolution du RSSI</h3>
        <div class="plot" id="rssi_evolution_plot"></div>
        
        <h3>Statistiques RSSI par station et AP</h3>
        <table>
            <thead>
                <tr>
                    <th>Station ID</th>
                    <th>AP ID</th>
                    <th>RSSI Moyen (dBm)</th>
                    <th>RSSI Min (dBm)</th>
                    <th>RSSI Max (dBm)</th>
                </tr>
            </thead>
            <tbody>
                {% for _, row in rssi_stats.iterrows() %}
                <tr>
                    <td>{{ row['StationID'] }}</td>
                    <td>{{ row['APID'] }}</td>
                    <td>{{ row['Mean RSSI']|round(2) }}</td>
                    <td>{{ row['Min RSSI']|round(2) }}</td>
                    <td>{{ row['Max RSSI']|round(2) }}</td>
                </tr>
                {% endfor %}
            </tbody>
        </table>
        
        <h3>Distribution du RSSI</h3>
        <div class="plot" id="rssi_distribution_plot"></div>
        
        {% if 'rssi_heatmap' in graphs %}
        <h3>Carte de chaleur RSSI (Station 0)</h3>
        <div class="plot" id="rssi_heatmap_plot"></div>
        {% endif %}
    </div>
    {% endif %}
    
    {% if has_handover %}
    <div class="section">
        <h2>Analyse des Handovers</h2>
        <p>Cette section présente l'analyse des événements de handover au cours de la simulation.</p>
        
        <h3>Statistiques des handovers</h3>
        <table>
            <thead>
                <tr>
                    <th>Métrique</th>
                    <th>Valeur</th>
                </tr>
            </thead>
            <tbody>
                {% for _, row in handover_stats.iterrows() %}
                <tr>
                    <td>{{ row['Métrique'] }}</td>
                    <td>{{ row['Valeur']|round(4) if row['Valeur'] is number else row['Valeur'] }}</td>
                </tr>
                {% endfor %}
            </tbody>
        </table>
        
        <h3>Chronologie des événements</h3>
        <div class="plot" id="handover_timeline_plot"></div>
        
        {% if 'handovers_by_station' in graphs %}
        <h3>Handovers par station</h3>
        <div class="plot" id="handovers_by_station_plot"></div>
        {% endif %}
        
        {% if 'handover_map' in graphs %}
        <h3>Carte des positions lors des handovers</h3>
        <div class="plot" id="handover_map_plot"></div>
        {% endif %}
    </div>
    {% endif %}
    
    {% if has_flow %}
    <div class="section">
        <h2>Analyse des Flux</h2>
        <p>Cette section présente l'analyse des performances réseau pour les différents flux.</p>
        
        <h3>Statistiques des flux</h3>
        <table>
            <thead>
                <tr>
                    <th>Métrique</th>
                    <th>Valeur</th>
                </tr>
            </thead>
            <tbody>
                {% for _, row in flow_stats.iterrows() %}
                <tr>
                    <td>{{ row['Métrique'] }}</td>
                    <td>{{ row['Valeur']|round(4) if row['Valeur'] is number else row['Valeur'] }}</td>
                </tr>
                {% endfor %}
            </tbody>
        </table>
        
        {% if 'throughput' in graphs %}
        <h3>Débit par flux</h3>
        <div class="plot" id="throughput_plot"></div>
        {% endif %}
        
        {% if 'packet_loss' in graphs %}
        <h3>Taux de perte par flux</h3>
        <div class="plot" id="packet_loss_plot"></div>
        {% endif %}
        
        {% if 'delay' in graphs %}
        <h3>Délai moyen par flux</h3>
        <div class="plot" id="delay_plot"></div>
        {% endif %}
    </div>
    {% endif %}
    
    <div class="footer">
        <p>Rapport généré le {{ generation_date }}</p>
    </div>
    
    <script>
        // Insertion des données JSON pour les graphiques
        {% if has_rssi and 'rssi_evolution' in graphs %}
        var rssi_evolution_data = {{ graphs.rssi_evolution | safe }};
        Plotly.newPlot('rssi_evolution_plot', rssi_evolution_data.data, rssi_evolution_data.layout);
        {% endif %}
        
        {% if has_rssi and 'rssi_distribution' in graphs %}
        var rssi_distribution_data = {{ graphs.rssi_distribution | safe }};
        Plotly.newPlot('rssi_distribution_plot', rssi_distribution_data.data, rssi_distribution_data.layout);
        {% endif %}
        
        {% if has_rssi and 'rssi_heatmap' in graphs %}
        var rssi_heatmap_data = {{ graphs.rssi_heatmap | safe }};
        Plotly.newPlot('rssi_heatmap_plot', rssi_heatmap_data.data, rssi_heatmap_data.layout);
        {% endif %}
        
        {% if has_handover and 'handover_timeline' in graphs %}
        var handover_timeline_data = {{ graphs.handover_timeline | safe }};
        Plotly.newPlot('handover_timeline_plot', handover_timeline_data.data, handover_timeline_data.layout);
        {% endif %}
        
        {% if has_handover and 'handovers_by_station' in graphs %}
        var handovers_by_station_data = {{ graphs.handovers_by_station | safe }};
        Plotly.newPlot('handovers_by_station_plot', handovers_by_station_data.data, handovers_by_station_data.layout);
        {% endif %}
        
        {% if has_handover and 'handover_map' in graphs %}
        var handover_map_data = {{ graphs.handover_map | safe }};
        Plotly.newPlot('handover_map_plot', handover_map_data.data, handover_map_data.layout);
        {% endif %}
        
        {% if has_flow and 'throughput' in graphs %}
        var throughput_data = {{ graphs.throughput | safe }};
        Plotly.newPlot('throughput_plot', throughput_data.data, throughput_data.layout);
        {% endif %}
        
        {% if has_flow and 'packet_loss' in graphs %}
        var packet_loss_data = {{ graphs.packet_loss | safe }};
        Plotly.newPlot('packet_loss_plot', packet_loss_data.data, packet_loss_data.layout);
        {% endif %}
        
        {% if has_flow and 'delay' in graphs %}
        var delay_data = {{ graphs.delay | safe }};
        Plotly.newPlot('delay_plot', delay_data.data, delay_data.layout);
        {% endif %}
    </script>
</body>
</html>"""

    # Préparer les données pour le template
    template_data = {
        'rssi_file': rssi_file,
        'handover_file': handover_file,
        'flow_file': flow_file,
        'has_rssi': data['rssi'] is not None and 'rssi_evolution' in all_graphs,
        'has_handover': data['handover'] is not None and 'handover_timeline' in all_graphs,
        'has_flow': data['flow'] is not None and ('throughput' in all_graphs or 'packet_loss' in all_graphs or 'delay' in all_graphs),
        'rssi_count': len(data['rssi']) if data['rssi'] is not None else 0,
        'handover_count': len(data['handover'][data['handover']['EventType'] == 'HANDOVER']) if data['handover'] is not None else 0,
        'flow_count': len(data['flow']) if data['flow'] is not None else 0,
        'avg_throughput': all_stats.get('flow_stats', {}).get('Valeur', [None])[1] if all_stats.get('flow_stats') is not None else None,
        'rssi_stats': all_stats.get('rssi_stats', pd.DataFrame()),
        'handover_stats': all_stats.get('handover_stats', pd.DataFrame()),
        'flow_stats': all_stats.get('flow_stats', pd.DataFrame()),
        'graphs': {k: v.to_json() for k, v in all_graphs.items()},
        'generation_date': datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }
    
    # Créer le rapport HTML
    env = jinja2.Environment()
    template = env.from_string(template_str)
    html_content = template.render(**template_data)
    
    # Enregistrer le rapport HTML
    with open(output_file, 'w') as f:
        f.write(html_content)
    
    print(f"\n✅ Rapport HTML généré: {output_file}")
    return output_file

def main():
    """Fonction principale"""
    # Configurer les arguments en ligne de commande
    parser = argparse.ArgumentParser(description='Analyse les données de simulation WiFi Roaming')
    parser.add_argument('--rssi', default='rssi_measurements.csv', help='Fichier des mesures RSSI')
    parser.add_argument('--handover', default='handover_events.csv', help='Fichier des événements de handover')
    parser.add_argument('--flow', default='flow_stats.csv', help='Fichier des statistiques de flux')
    parser.add_argument('--output', default='wifi_analysis_report.html', help='Fichier de sortie HTML')
    parser.add_argument('--show', action='store_true', help='Ouvrir le rapport dans un navigateur')
    
    args = parser.parse_args()
    
    print("\n=== Analyse des données de simulation WiFi ===\n")
    
    # Charger les données
    data = load_data(args.rssi, args.handover, args.flow)
    
    # Analyser les données
    all_graphs = {}
    all_stats = {}
    
    # RSSI
    rssi_graphs, rssi_stats = analyze_rssi(data)
    if rssi_graphs:
        all_graphs.update(rssi_graphs)
        all_stats['rssi_stats'] = rssi_stats
    
    # Handovers
    handover_graphs, handover_stats = analyze_handovers(data)
    if handover_graphs:
        all_graphs.update(handover_graphs)
        all_stats['handover_stats'] = handover_stats
    
    # Flux
    flow_graphs, flow_stats = analyze_flows(data)
    if flow_graphs:
        all_graphs.update(flow_graphs)
        all_stats['flow_stats'] = flow_stats
    
    # Générer le rapport HTML
    output_file = create_html_report(data, all_graphs, all_stats, args.output, args.rssi, args.handover, args.flow)
    
    # Ouvrir le rapport dans un navigateur si demandé
    if args.show:
        import webbrowser
        webbrowser.open(output_file)

if __name__ == "__main__":
    main()
