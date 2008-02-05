// [Name] SVGCircleElement-svgdom-r-prop.js
// [Expected rendering result] green circle - and a series of PASS mesages

description("Tests dynamic updates of the 'r' property of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "0");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBe("circleElement.r.baseVal.value", "0");

function executeTest() {
    circleElement.r.baseVal.value = "150";
    shouldBe("circleElement.r.baseVal.value", "150");

    waitForClickEvent(circleElement); 
    triggerUpdate();
}

window.setTimeout("executeTest()", 0);
var successfullyParsed = true;
