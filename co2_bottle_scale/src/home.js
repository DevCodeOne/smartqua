import { Component } from 'preact';
import { ScaleContext } from './scalecontext';

export class Home extends Component {

    static contextType = ScaleContext;

    state = {
        load : '',
        containedCo2 : 0
    }

    componentDidMount() {
        const { apiAddress } = this.context;

        this.timer = setInterval( () => {
            fetch (apiAddress + "/api/v1/load", {mode: 'cors'})
                .then(response => response.json())
                .then(json => { this.setState({load: json.load, containedCo2 : json.contained_co2}); console.log(json); });
        }, 1000);
    }

    componentWillUnmount() {
        clearInterval(this.timer);
    }

	render() {
		return (
            <div class="container">
                <div class="row">
			        <h3>Used Co2: {Math.abs(this.state.load)}g </h3>
                </div>
                <div class="row">
			        <h3>Still available Co2: {this.state.containedCo2 - Math.abs(this.state.load)}g </h3>
                </div>
            </div>
		);
	}
};
