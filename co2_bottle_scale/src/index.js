import './style';
import { Component } from 'preact';

export default class App extends Component {

    state = {
        load : ''
    }

    onClick = ev => {
        console.log("Test");
    }

    componentDidMount() {
        this.timer = setInterval( () => {
            fetch ("http://first-co2-bottle-scale.fritz.box/api/v1/load", {mode: 'cors'})
                .then(response => response.json())
                .then(json => { this.setState({load: json.load}); console.log(json); });
        }, 1000);
    }

    componentWillUnmount() {
        clearInterval(this.timer);
    }

	render() {
		return (
			<div>
				<h1>Used Co2: {this.state.load} </h1>
                <button onclick={this.onClick}> Tare </button>
			</div>
		);
	}
}
